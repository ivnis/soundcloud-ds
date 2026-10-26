#include "endpoint.h"
#include <fat.h>
#include <nds/arm9/dldi.h>
#include <cstdio>

namespace { bool storage = false; }

bool loadPreferences() {
    const DISC_INTERFACE* disk = dldiGetInternal();
    if (!disk || !(disk->features & FEATURE_MEDIUM_CANREAD)) return false;
    storage = fatInitDefault();
    if (!storage) return false;
    FILE* file = std::fopen("/soundcloud-ds.cfg", "r");
    if (!file) return false;
    char value[kEndpointTextCapacity]{}, mode[16]{}, other[kEndpointTextCapacity]{};
    bool read = std::fgets(value, sizeof(value), file) != nullptr;
    bool profiles = std::fgets(mode, sizeof(mode), file) && std::fgets(other, sizeof(other), file);
    std::fclose(file);
    ServerEndpoint parsed{}, alternate{};
    if (!read || !parseEndpoint(value, parsed)) return false;
    bool emulator = !profiles || mode[0] != '0';
    selectEmulatorMode(emulator); setEndpoint(parsed);
    if (profiles && parseEndpoint(other, alternate)) {
        selectEmulatorMode(!emulator); setEndpoint(alternate); selectEmulatorMode(emulator);
    }
    return true;
}

bool savePreferences() {
    if (!storage) return false;
    FILE* file = std::fopen("/soundcloud-ds.cfg", "w");
    if (!file) return false;
    bool emulator = emulatorMode();
    char text[kEndpointTextCapacity], other[kEndpointTextCapacity]; formatEndpoint(currentEndpoint(), text, sizeof(text));
    selectEmulatorMode(!emulator); formatEndpoint(currentEndpoint(), other, sizeof(other));
    selectEmulatorMode(emulator);
    bool written = std::fprintf(file, "%s\n%d\n%s\n", text, emulator ? 1 : 0, other) > 0;
    return std::fclose(file) == 0 && written;
}
