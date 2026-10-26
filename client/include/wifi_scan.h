#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

inline bool hiddenWifiName(const char* ssid,unsigned length){return length==0 || !ssid[0];}
inline void formatWifiName(char* output,std::size_t capacity,const char* ssid,
                           unsigned length,const uint8_t bssid[6]){
    if(!capacity)return;
    if(hiddenWifiName(ssid,length)){
        std::snprintf(output,capacity,"Hidden %02X:%02X:%02X",bssid[3],bssid[4],bssid[5]);
        return;
    }
    std::size_t bytes=std::min<std::size_t>({length,32,capacity-1});
    std::memcpy(output,ssid,bytes);output[bytes]=0;
    for(std::size_t i=0;i<bytes;++i)
        if(static_cast<unsigned char>(output[i])<32 || output[i]==127)output[i]='?';
}
