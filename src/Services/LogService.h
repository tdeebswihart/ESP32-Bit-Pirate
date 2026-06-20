#pragma once
#include "Services/SdService.h"
#include <esp_log.h>
#include <cstdarg>

class LogService {
public:
    explicit LogService(SdService& sd);

    void begin();
    void end();
    bool isEnabled() const;

    size_t      fileSize(uint8_t slot);
    std::string readChunk(uint8_t slot, size_t offset, size_t maxBytes);

    static const char* slotPath(uint8_t slot);

private:
    SdService& sdService;
    bool       enabled = false;
    File       logFile;
    void rotate();
    void flushBuffer();

    static LogService*    activeLogger;
    static char           accumBuf[512];
    static size_t         accumLen;
    static vprintf_like_t originalVprintf;

    static int sdLogVprintf(const char* fmt, va_list args);
};
