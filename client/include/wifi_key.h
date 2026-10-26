#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

enum class WifiSecurity { Open, Wep, Wpa2, Unsupported };
struct WifiKey { uint8_t bytes[64]{}; std::size_t length=0; };

inline bool supportsWifiNetwork(WifiSecurity security,bool compatible,bool dsi,
                                bool personal,bool aes) {
    if(!compatible)return false;
    if(security==WifiSecurity::Open || security==WifiSecurity::Wep)return true;
    return security==WifiSecurity::Wpa2 && dsi && personal && aes;
}

inline void wipeWifiSecret(void* data,std::size_t size) {
    auto* bytes=static_cast<volatile uint8_t*>(data);
    while(size--)*bytes++=0;
}
inline int wifiHex(char c) {
    return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;
}
// No trimming or case conversion: spaces and letter case are part of a key.
inline bool parseWifiKey(WifiSecurity security,const char* text,WifiKey& key) {
    wipeWifiSecret(&key,sizeof(key));
    std::size_t size=std::strlen(text);
    if(security==WifiSecurity::Open)return size==0;
    if(security==WifiSecurity::Wep && (size==10 || size==26)){
        for(std::size_t i=0;i<size;i+=2){
            int a=wifiHex(text[i]),b=wifiHex(text[i+1]);
            if(a<0 || b<0){wipeWifiSecret(&key,sizeof(key));return false;}
            key.bytes[i/2]=static_cast<uint8_t>(a*16+b);
        }
        key.length=size/2;return true;
    }
    if(security==WifiSecurity::Wep){if(size!=5 && size!=13)return false;}
    else if(security==WifiSecurity::Wpa2){if(size<8 || size>63)return false;}
    else return false;
    for(std::size_t i=0;i<size;++i)
        if(static_cast<unsigned char>(text[i])<32 || static_cast<unsigned char>(text[i])>126)return false;
    std::memcpy(key.bytes,text,size);key.length=size;return true;
}
