#pragma once

// Copy config.local.example.h to config.local.h to override these defaults.
#if __has_include("config.local.h")
#include "config.local.h"
#endif
#ifndef SC_SERVER_IP
#define SC_SERVER_IP "192.168.1.100"
#endif
#ifndef SC_SERVER_PORT
#define SC_SERVER_PORT 8000
#endif

constexpr unsigned kSampleRate = 22050;
constexpr unsigned kBytesPerSecond = kSampleRate * 2;
constexpr unsigned kHistoryBytes = kBytesPerSecond * 6;
constexpr unsigned kAheadBytes = kBytesPerSecond * 20;
constexpr unsigned kBufferBytes = kHistoryBytes + kAheadBytes;
constexpr unsigned kPrefillBytes = kBytesPerSecond * 6;
constexpr unsigned rebufferBytes(unsigned gaps) {
    return kBytesPerSecond * (gaps < 3 ? 6 + gaps * 2 : 12);
}
