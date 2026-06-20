#include "Services/OSCService.h"

#include <Arduino.h>
#include <OSCMessage.h>
#include <Print.h>
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/inet.h"
#include <cstring>
#include <cstdlib>
#include <cctype>

// ---------------------------------------------------------------------------
// VecPrint: thin Print adapter that captures bytes into a std::vector
// ---------------------------------------------------------------------------
class VecPrint : public Print {
public:
    std::vector<uint8_t> buf;

    size_t write(uint8_t b) override {
        buf.push_back(b);
        return 1;
    }

    size_t write(const uint8_t* b, size_t n) override {
        buf.insert(buf.end(), b, b + n);
        return n;
    }
};

// ---------------------------------------------------------------------------
// Argument type detection and OSC value insertion
//
// Priority (first match wins):
//   1. 0x[hex]       -> blob
//   2. [num]f        -> float  (e.g. 133f, -3.14f)
//   3. u[digits]     -> uint32 cast to int32
//   4. [num].[num]   -> float  (e.g. 3.14, 120.)
//   5. i?[-][digits] -> int32  (i optional; bare integer)
//   6. anything else -> string
// ---------------------------------------------------------------------------
static bool isDigitsOnly(const std::string& s, size_t start = 0) {
    if (start >= s.size()) return false;
    for (size_t i = start; i < s.size(); i++)
        if (!isdigit((unsigned char)s[i])) return false;
    return true;
}

static void addOscArg(OSCMessage& msg, const std::string& token) {
    if (token.empty()) {
        msg.add("");
        return;
    }

    // 1. Blob: 0x[0-9a-fA-F]+
    if (token.size() >= 3 && token[0] == '0' && (token[1] == 'x' || token[1] == 'X')) {
        std::string hex = token.substr(2);
        if (hex.size() % 2 != 0) hex = "0" + hex;
        std::vector<uint8_t> blob;
        blob.reserve(hex.size() / 2);
        for (size_t i = 0; i < hex.size(); i += 2) {
            unsigned long byte = strtoul(hex.substr(i, 2).c_str(), nullptr, 16);
            blob.push_back((uint8_t)byte);
        }
        msg.add(blob.data(), (int)blob.size());
        return;
    }

    // 2. Float with 'f' suffix: e.g. 133f, -3.14f
    if (token.size() >= 2 && token.back() == 'f') {
        std::string num = token.substr(0, token.size() - 1);
        size_t start = (!num.empty() && num[0] == '-') ? 1 : 0;
        int dots = 0;
        bool valid = (num.size() > start);
        for (size_t i = start; i < num.size() && valid; i++) {
            if (num[i] == '.') { if (++dots > 1) valid = false; }
            else if (!isdigit((unsigned char)num[i])) valid = false;
        }
        if (valid) {
            msg.add((float)atof(num.c_str()));
            return;
        }
    }

    // 3. Uint: u[digits]
    if (token[0] == 'u' && isDigitsOnly(token, 1)) {
        uint32_t v = (uint32_t)strtoul(token.c_str() + 1, nullptr, 10);
        msg.add((int32_t)v);
        return;
    }

    // 4. Float with decimal: [opt-minus][digits].[digits]
    if (token.find('.') != std::string::npos) {
        size_t start = (!token.empty() && token[0] == '-') ? 1 : 0;
        int dots = 0;
        bool valid = (token.size() > start);
        for (size_t i = start; i < token.size() && valid; i++) {
            if (token[i] == '.') { if (++dots > 1) valid = false; }
            else if (!isdigit((unsigned char)token[i])) valid = false;
        }
        if (valid && dots == 1) {
            msg.add((float)atof(token.c_str()));
            return;
        }
    }

    // 5. Int: [i][opt-minus][digits]
    {
        std::string num = token;
        if (!num.empty() && num[0] == 'i') num = num.substr(1);
        size_t start = (!num.empty() && num[0] == '-') ? 1 : 0;
        if (num.size() > start && isDigitsOnly(num, start)) {
            msg.add((int32_t)atoi(num.c_str()));
            return;
        }
    }

    // 6. String fallback
    msg.add(token.c_str());
}

// ---------------------------------------------------------------------------
// DNS resolution via lwIP getaddrinfo
// ---------------------------------------------------------------------------
bool OSCService::resolveHost(const std::string& host, uint32_t& ipv4Out) {
    struct addrinfo hints = {};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    struct addrinfo* res = nullptr;
    int rc = getaddrinfo(host.c_str(), nullptr, &hints, &res);
    if (rc != 0 || !res) return false;

    ipv4Out = ((struct sockaddr_in*)res->ai_addr)->sin_addr.s_addr;
    freeaddrinfo(res);
    return true;
}

// ---------------------------------------------------------------------------
// Build and serialize an OSCMessage into a byte vector
// ---------------------------------------------------------------------------
bool OSCService::buildMessage(const std::string& oscAddress,
                               const std::vector<std::string>& argTokens,
                               std::vector<uint8_t>& outBytes,
                               std::string& errorOut)
{
    OSCMessage msg(oscAddress.c_str());

    for (const auto& tok : argTokens)
        addOscArg(msg, tok);

    if (msg.hasError()) {
        errorOut = "OSCMessage allocation error";
        return false;
    }

    VecPrint vp;
    msg.send(vp);

    if (vp.buf.empty()) {
        errorOut = "OSCMessage serialized to zero bytes";
        return false;
    }

    outBytes = std::move(vp.buf);
    return true;
}

// ---------------------------------------------------------------------------
// UDP send
// ---------------------------------------------------------------------------
bool OSCService::sendUDP(const std::string& host, uint16_t port,
                          const std::string& oscAddress,
                          const std::vector<std::string>& argTokens,
                          std::string& errorOut)
{
    uint32_t ip;
    if (!resolveHost(host, ip)) {
        errorOut = "Failed to resolve: " + host;
        return false;
    }

    std::vector<uint8_t> bytes;
    if (!buildMessage(oscAddress, argTokens, bytes, errorOut)) return false;

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        errorOut = "socket() failed";
        return false;
    }

    struct sockaddr_in dest = {};
    dest.sin_family      = AF_INET;
    dest.sin_addr.s_addr = ip;
    dest.sin_port        = htons(port);

    ssize_t sent = sendto(sock, bytes.data(), bytes.size(), 0,
                          (struct sockaddr*)&dest, sizeof(dest));
    close(sock);

    if (sent < 0 || (size_t)sent != bytes.size()) {
        errorOut = "sendto() failed";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// TCP send — OSC framing: 4-byte big-endian payload size, then payload
// ---------------------------------------------------------------------------
bool OSCService::sendTCP(const std::string& host, uint16_t port,
                          const std::string& oscAddress,
                          const std::vector<std::string>& argTokens,
                          std::string& errorOut)
{
    uint32_t ip;
    if (!resolveHost(host, ip)) {
        errorOut = "Failed to resolve: " + host;
        return false;
    }

    std::vector<uint8_t> bytes;
    if (!buildMessage(oscAddress, argTokens, bytes, errorOut)) return false;

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        errorOut = "socket() failed";
        return false;
    }

    // 3-second send/recv timeout
    struct timeval tv = { 3, 0 };
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in dest = {};
    dest.sin_family      = AF_INET;
    dest.sin_addr.s_addr = ip;
    dest.sin_port        = htons(port);

    if (connect(sock, (struct sockaddr*)&dest, sizeof(dest)) != 0) {
        close(sock);
        errorOut = "connect() failed";
        return false;
    }

    // OSC-over-TCP: 4-byte big-endian message length prefix
    uint32_t msgLen = htonl((uint32_t)bytes.size());
    if (send(sock, &msgLen, 4, 0) != 4) {
        close(sock);
        errorOut = "send() length prefix failed";
        return false;
    }

    if (send(sock, bytes.data(), bytes.size(), 0) != (ssize_t)bytes.size()) {
        close(sock);
        errorOut = "send() payload failed";
        return false;
    }

    close(sock);
    return true;
}

// ---------------------------------------------------------------------------
// Help text
// ---------------------------------------------------------------------------
std::string OSCService::getHelpText() {
    return
        "\nOSC (Open Sound Control) Client\n\n"
        "Usage:\n"
        "  osc [-u|--udp|-t|--tcp] HOST[:PORT] /address [args...]\n"
        "  osc (transport|tport|tp) (udp|tcp)\n"
        "  osc port <PORT>\n\n"
        "Defaults: UDP, port 8888\n\n"
        "Transport flags:\n"
        "  -u, --udp   UDP (default)\n"
        "  -t, --tcp   TCP with 4-byte size prefix\n\n"
        "Argument types:\n"
        "  hello        string\n"
        "  42  / i42    int32\n"
        "  u42          uint32 (stored as int32)\n"
        "  3.14         float\n"
        "  99f          float (f suffix)\n"
        "  0xDEADBEEF   blob\n\n"
        "Examples:\n"
        "  osc 192.168.1.10:8000 /synth/note 60 i100\n"
        "  osc -t 192.168.1.10 /set/volume 0.75f\n"
        "  osc transport tcp\n"
        "  osc port 9000\n";
}
