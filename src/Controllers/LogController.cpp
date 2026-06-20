#include "LogController.h"
#include <string>
#include <algorithm>

LogController::LogController(ITerminalView& tv, LogService& ls, SdService& sd)
    : terminalView(tv), logService(ls), sdService(sd) {}

void LogController::handleCommand(const TerminalCommand& cmd) {
    const auto& root = cmd.getRoot();
    if      (root == "on")   handleOn();
    else if (root == "off")  handleOff();
    else if (root == "head") handleHead(cmd);
    else if (root == "tail") handleTail(cmd);
    else if (root == "ls")   handleLs();
    else                     handleHelp();
}

void LogController::ensureConfigured() {
    if (logService.isEnabled()) {
        terminalView.println("LOG mode. Logging is ON -> /logs/boot0.log");
    } else {
        terminalView.println("LOG mode. Logging is OFF. Type 'on' to start.");
    }
    if (!sdService.getSdState()) {
        terminalView.println("Warning: SD card not mounted.");
    }
}

void LogController::handleOn() {
    if (!sdService.getSdState()) {
        terminalView.println("Error: SD card not mounted.");
        return;
    }
    if (logService.isEnabled()) {
        terminalView.println("Logging already enabled.");
        return;
    }
    logService.begin();
    if (logService.isEnabled()) {
        terminalView.println("Logging started -> /logs/boot0.log");
    } else {
        terminalView.println("Error: Failed to open log file.");
    }
}

void LogController::handleOff() {
    if (!logService.isEnabled()) {
        terminalView.println("Logging already disabled.");
        return;
    }
    logService.end();
    terminalView.println("Logging stopped.");
}

void LogController::handleLs() {
    for (uint8_t i = 0; i < 4; ++i) {
        std::string path = LogService::slotPath(i);
        if (sdService.isFile(path)) {
            size_t sz = logService.fileSize(i);
            terminalView.println("  " + path + "  (" + std::to_string(sz) + " bytes)");
        } else {
            terminalView.println("  " + path + "  (absent)");
        }
    }
}

void LogController::handleHead(const TerminalCommand& cmd) {
    size_t n = parseN(cmd, 20);
    if (!sdService.isFile(LogService::slotPath(0))) {
        terminalView.println(std::string("No log file found (") + LogService::slotPath(0) + ").");
        return;
    }

    size_t offset = 0;
    size_t linesShown = 0;
    std::string partial;

    while (linesShown < n) {
        std::string chunk = logService.readChunk(0, offset, 512);
        if (chunk.empty()) break;
        offset += chunk.size();

        partial += chunk;
        size_t pos = 0;
        size_t nl;
        while ((nl = partial.find('\n', pos)) != std::string::npos && linesShown < n) {
            terminalView.println(partial.substr(pos, nl - pos));
            pos = nl + 1;
            ++linesShown;
        }
        partial = partial.substr(pos);
        if (partial.size() > 4096) {
            terminalView.println(partial.substr(0, 4096) + " [line truncated]");
            partial.clear();
            ++linesShown;
        }
    }
    if (!partial.empty() && linesShown < n) {
        terminalView.println(partial);
    }
}

void LogController::handleTail(const TerminalCommand& cmd) {
    size_t n     = parseN(cmd, 20);
    size_t total = logService.fileSize(0);
    if (total == 0) {
        terminalView.println("Log file is empty.");
        return;
    }

    size_t newlinesFound = 0;
    size_t startOffset   = 0;
    bool   found         = false;

    // Skip a trailing newline so it isn't counted as an empty line
    std::string lastByte = logService.readChunk(0, total - 1, 1);
    size_t scanEnd = (lastByte == "\n") ? total - 1 : total;

    size_t pos = scanEnd;
    while (pos > 0 && !found) {
        size_t chunkSize = std::min(pos, (size_t)512);
        pos -= chunkSize;
        std::string chunk = logService.readChunk(0, pos, chunkSize);

        for (int i = (int)chunk.size() - 1; i >= 0; --i) {
            if (chunk[i] == '\n') {
                ++newlinesFound;
                if (newlinesFound > n) {
                    startOffset = pos + (size_t)i + 1;
                    found = true;
                    break;
                }
            }
        }
    }

    size_t readPos = startOffset;
    while (readPos < total) {
        std::string chunk = logService.readChunk(0, readPos, 512);
        if (chunk.empty()) break;
        readPos += chunk.size();
        terminalView.print(chunk);
    }
    terminalView.println("");
}

size_t LogController::parseN(const TerminalCommand& cmd, size_t defaultN) const {
    const std::string& sub = cmd.getSubcommand();
    if (sub.empty()) return defaultN;
    try {
        int v = std::stoi(sub);
        if (v > 0) return (size_t)v;
    } catch (...) {}
    return defaultN;
}

void LogController::handleHelp() {
    terminalView.println("\nLOG commands:");
    terminalView.println("  on          Start SD logging -> /logs/boot0.log");
    terminalView.println("  off         Stop SD logging");
    terminalView.println("  head [n]    Print first n lines of boot0.log (default 20)");
    terminalView.println("  tail [n]    Print last n lines of boot0.log (default 20)");
    terminalView.println("  ls          List all log slots with sizes");
    terminalView.println("  help        Show this message\n");
}
