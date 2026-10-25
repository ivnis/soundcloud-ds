#include "endpoint.h"
#include "config.h"
#include <cstdio>
#include <cstring>

namespace {
bool emulator = true;
ServerEndpoint profiles[2] = {{SC_SERVER_IP, SC_SERVER_PORT}, {"10.64.0.1", 8765}};
}

bool parseEndpoint(const char* text, ServerEndpoint& result) {
    while (*text == ' ' || *text == '\t') ++text;
    if (std::strncmp(text, "http://", 7) == 0) text += 7;
    unsigned octets[4]{};
    for (unsigned i = 0; i < 4; ++i) {
        unsigned digits = 0;
        while (*text >= '0' && *text <= '9') {
            if (++digits > 3) return false;
            octets[i] = octets[i] * 10 + *text++ - '0';
        }
        if (!digits || octets[i] > 255 || *text++ != (i == 3 ? ':' : '.')) return false;
    }
    unsigned port = 0, digits = 0;
    while (*text >= '0' && *text <= '9') {
        if (++digits > 5) return false;
        port = port * 10 + *text++ - '0';
    }
    if (!digits || port == 0 || port > 65535) return false;
    if (*text == '/') ++text;
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') ++text;
    if (*text) return false;
    ServerEndpoint parsed{};
    std::snprintf(parsed.host, sizeof(parsed.host), "%u.%u.%u.%u", octets[0], octets[1], octets[2], octets[3]);
    parsed.port = port;
    result = parsed;
    return true;
}
void formatEndpoint(const ServerEndpoint& value, char* text, std::size_t capacity) {
    std::snprintf(text, capacity, "%s:%u", value.host, value.port);
}
const ServerEndpoint& currentEndpoint() { return profiles[emulator ? 1 : 0]; }
void setEndpoint(const ServerEndpoint& endpoint) { profiles[emulator ? 1 : 0] = endpoint; }
bool emulatorMode() { return emulator; }
void selectEmulatorMode(bool value) { emulator = value; }
