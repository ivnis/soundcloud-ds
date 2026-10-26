#pragma once
#include <algorithm>

// Read as fast as the socket and free PCM buffer allow, using substantial
// bursts. Bounds include a pending ADPCM block and 4x expansion.
class AudioDownload {
public:
    unsigned budget(unsigned pcmRoom) {
        unsigned wire=pcmRoom>1024 ? (pcmRoom-1024)/4 : 0;
        if (wire<2048) return 0;
        return std::min(wire,8192u);
    }
};
