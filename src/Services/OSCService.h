#pragma once

#include <string>
#include <vector>
#include <cstdint>

// OSC client service (UDP and TCP).
// Wraps the CNMAT/OSC library for use over Wi-Fi or Ethernet.
//
// Argument token format (parsed in buildMessage):
//   0x[hex]         -> blob
//   [digits]f       -> float (e.g. 133f)
//   [digits].[...] -> float (e.g. 3.14)
//   u[digits]       -> uint32 stored as int32 (OSC has no uint tag)
//   i?[digits]      -> int32 (i prefix optional; bare numbers default to int)
//   anything else   -> string
class OSCService {
public:
    OSCService() = default;

    // Send an OSC message via UDP (fire-and-forget).
    bool sendUDP(const std::string& host, uint16_t port,
                 const std::string& oscAddress,
                 const std::vector<std::string>& argTokens,
                 std::string& errorOut);

    // Send an OSC message via TCP (4-byte big-endian size-prefixed framing).
    bool sendTCP(const std::string& host, uint16_t port,
                 const std::string& oscAddress,
                 const std::vector<std::string>& argTokens,
                 std::string& errorOut);

    static std::string getHelpText();

private:
    bool resolveHost(const std::string& host, uint32_t& ipv4Out);
    bool buildMessage(const std::string& oscAddress,
                      const std::vector<std::string>& argTokens,
                      std::vector<uint8_t>& outBytes,
                      std::string& errorOut);
};
