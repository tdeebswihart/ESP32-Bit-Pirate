#include "LogService.h"
#include <cstdio>

LogService*    LogService::activeLogger    = nullptr;
char           LogService::accumBuf[512]   = {};
size_t         LogService::accumLen        = 0;
vprintf_like_t LogService::originalVprintf = nullptr;

static constexpr const char* SLOT_PATHS[] = {
    "/logs/boot0.log",
    "/logs/boot1.log",
    "/logs/boot2.log",
    "/logs/boot3.log"
};

LogService::LogService(SdService& sd) : sdService(sd) {}

const char* LogService::slotPath(uint8_t slot) {
    if (slot > 3) slot = 3;
    return SLOT_PATHS[slot];
}

void LogService::rotate() {
    sdService.deleteFile(slotPath(3));
    for (int i = 2; i >= 0; --i) {
        if (sdService.isFile(slotPath(i))) {
            sdService.renameFile(slotPath(i), slotPath(i + 1));
        }
    }
}

void LogService::flushBuffer() {
    if (accumLen == 0) return;
    if (logFile) {
        logFile.write(reinterpret_cast<const uint8_t*>(accumBuf), accumLen);
        logFile.flush();
    }
    accumLen = 0;
}

void LogService::begin() {
    if (enabled) return;
    if (!sdService.getSdState()) return;

    sdService.ensureDirectory("/logs");
    rotate();

    logFile = sdService.openFileWrite(slotPath(0));
    if (!logFile) return;

    enabled         = true;
    activeLogger    = this;
    originalVprintf = esp_log_set_vprintf(&sdLogVprintf);
}

void LogService::end() {
    if (!enabled) return;

    esp_log_set_vprintf(originalVprintf);
    originalVprintf = nullptr;
    activeLogger    = nullptr;

    flushBuffer();
    logFile.close();
    enabled = false;
}

bool LogService::isEnabled() const {
    return enabled;
}

size_t LogService::fileSize(uint8_t slot) {
    File f = sdService.openFileRead(slotPath(slot));
    if (!f) return 0;
    size_t sz = f.size();
    f.close();
    return sz;
}

std::string LogService::readChunk(uint8_t slot, size_t offset, size_t maxBytes) {
    return sdService.readFileChunk(slotPath(slot), offset, maxBytes);
}

int LogService::sdLogVprintf(const char* fmt, va_list args) {
    int ret = 0;
    if (originalVprintf) {
        va_list copy;
        va_copy(copy, args);
        ret = originalVprintf(fmt, copy);
        va_end(copy);
    }

    if (!activeLogger || !activeLogger->enabled) return ret;

    char lineBuf[256];
    va_list copy2;
    va_copy(copy2, args);
    int n = vsnprintf(lineBuf, sizeof(lineBuf), fmt, copy2);
    va_end(copy2);
    if (n <= 0) return ret;
    if (n >= (int)sizeof(lineBuf)) n = (int)sizeof(lineBuf) - 1;

    for (int i = 0; i < n; ++i) {
        if (accumLen >= sizeof(accumBuf)) {
            activeLogger->flushBuffer();
        }
        accumBuf[accumLen++] = lineBuf[i];
        if (lineBuf[i] == '\n') {
            activeLogger->flushBuffer();
        }
    }

    return ret;
}
