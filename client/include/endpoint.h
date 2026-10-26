#pragma once
#include <cstddef>
constexpr std::size_t kEndpointHostCapacity=254;
constexpr std::size_t kEndpointTextCapacity=272;
constexpr std::size_t kEndpointInputCapacity=288;

struct ServerEndpoint {
    char host[kEndpointHostCapacity];
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
