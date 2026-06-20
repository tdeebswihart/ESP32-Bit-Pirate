#pragma once
#include "Models/TerminalCommand.h"
#include "Interfaces/ITerminalView.h"
#include "Services/LogService.h"
#include "Services/SdService.h"

class LogController {
public:
    LogController(ITerminalView& terminalView,
                  LogService&    logService,
                  SdService&     sdService);

    void handleCommand(const TerminalCommand& cmd);
    void ensureConfigured();

private:
    void handleOn();
    void handleOff();
    void handleHead(const TerminalCommand& cmd);
    void handleTail(const TerminalCommand& cmd);
    void handleLs();
    void handleHelp();
    size_t parseN(const TerminalCommand& cmd, size_t defaultN) const;

    ITerminalView& terminalView;
    LogService&    logService;
    SdService&     sdService;
};
