#pragma once
#include "search.h"
#include <cstring>

// Search results are deliberately independent of this recommendation queue.
inline int nextRecommendation(const SearchResults& queue, const char* current,
                              const char recent[][24], unsigned count) {
    for (unsigned i=0;i<queue.count;++i) {
        const char* id=queue.tracks[i].id;
        bool seen=!*id || std::strcmp(id,current)==0;
        for(unsigned j=0;j<count;++j) if(std::strcmp(id,recent[j])==0) seen=true;
        if(!seen)return static_cast<int>(i);
    }
    return -1;
}
