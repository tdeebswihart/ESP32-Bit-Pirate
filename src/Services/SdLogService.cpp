#include "SdLogService.h"
#include <algorithm>
#include <cstdio>
#include <SD.h>

SdLogService* SdLogService::activeInstance = nullptr;

SdLogService::SdLogService(SdService& sdService) : sdService(sdService) {}

SdLogService::~SdLogService() {
    end();
}

bool SdLogService::begin(uint8_t clkPin, uint8_t misoPin, uint8_t mosiPin, uint8_t csPin,
                         const std::string& path) {
    if (active) end();

    if (!sdService.getSdState()) {
        if (!sdService.configure(clkPin, misoPin, mosiPin, csPin)) {
            return false;
        }
    }

    logFile = SD.open(path.c_str(), FILE_APPEND);
    if (!logFile) {
        return false;
    }

    active = true;
    activeInstance = this;
    prevHandler = esp_log_set_vprintf(logHandler);
    return true;
}

void SdLogService::end() {
    if (!active) return;

    esp_log_set_vprintf(prevHandler ? prevHandler : vprintf);
    prevHandler = nullptr;

    if (logFile) {
        logFile.flush();
        logFile.close();
    }

    active = false;
    activeInstance = nullptr;
}

int SdLogService::logHandler(const char* fmt, va_list args) {
    // Re-entrancy guard: SD file operations can internally call esp_log on error.
    // Without this, a failed write could recurse back into this handler.
    static bool inHandler = false;
    if (inHandler) {
        if (activeInstance && activeInstance->prevHandler) {
            return activeInstance->prevHandler(fmt, args);
        }
        return vprintf(fmt, args);
    }
    inHandler = true;

    int len = 0;

    if (activeInstance && activeInstance->active) {
        // Forward to the previous handler first (copy args since we'll also use them below)
        if (activeInstance->prevHandler) {
            va_list args_fwd;
            va_copy(args_fwd, args);
            len = activeInstance->prevHandler(fmt, args_fwd);
            va_end(args_fwd);
        }

        // Write to SD if still mounted
        if (activeInstance->sdService.getSdState() && activeInstance->logFile) {
            char buf[LOG_BUF_SIZE];
            int n = vsnprintf(buf, sizeof(buf), fmt, args);
            if (n > 0) {
                // vsnprintf may return a value >= LOG_BUF_SIZE on truncation;
                // cap to actual bytes written into buf.
                activeInstance->logFile.write(
                    reinterpret_cast<const uint8_t*>(buf),
                    std::min(static_cast<size_t>(n), LOG_BUF_SIZE - 1)
                );
            }
        } else if (activeInstance->active) {
            // SD was unmounted by another controller — deactivate cleanly
            if (activeInstance->logFile) {
                activeInstance->logFile.close();
            }
            activeInstance->active = false;
        }
    }

    inHandler = false;
    return len;
}
