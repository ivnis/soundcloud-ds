#pragma once
#include <cstddef>

struct ServerEndpoint {
    char host[16];
    unsigned port;
};
bool parseEndpoint(const char* text, ServerEndpoint& result);
void formatEndpoint(const ServerEndpoint& value, char* text, std::size_t capacity);
const ServerEndpoint& currentEndpoint();
void setEndpoint(const ServerEndpoint& endpoint);
bool emulatorMode();
void selectEmulatorMode(bool emulator);
bool loadPreferences();
bool savePreferences();
