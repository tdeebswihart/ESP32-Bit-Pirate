#pragma once

#include <cstdarg>
#include <string>
#include <esp_log.h>
#include <SD.h>
#include "SdService.h"

class SdLogService {
public:
    explicit SdLogService(SdService& sdService);
    ~SdLogService();

    // Try to mount SD (if not already), open log file in append mode, and install
    // our vprintf hook. Returns false if SD is unavailable.
    bool begin(uint8_t clkPin, uint8_t misoPin, uint8_t mosiPin, uint8_t csPin,
               const std::string& path = "/log.txt");

    void end();
    bool isActive() const { return active; }

private:
    static int logHandler(const char* fmt, va_list args);
    static SdLogService* activeInstance;

    SdService& sdService;
    File logFile;
    vprintf_like_t prevHandler = nullptr;
    bool active = false;

    static constexpr size_t LOG_BUF_SIZE = 256;
};
