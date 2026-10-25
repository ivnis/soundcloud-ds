#pragma once
#include <cstddef>
constexpr unsigned kSearchPageSize = 4;

struct TrackItem {
    char id[24]{};
    char title[481]{};
    char artist[321]{};
    unsigned durationMs = 0;
};

struct SearchResults {
    TrackItem tracks[12]{};
    unsigned count = 0, offset = 0, nextOffset = 0;
    bool hasMore = false;
};

bool parseSearch(const char* json, std::size_t length, SearchResults& result);
bool parseHealth(const char* json, std::size_t length);
