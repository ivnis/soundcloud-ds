#include "endpoint.h"
#include "config.h"
#include <cstdio>
#include <cstring>

namespace {
bool emulator = true;
ServerEndpoint profiles[2] = {{SC_SERVER_IP, SC_SERVER_PORT}, {"10.64.0.1", 8765}};
}

bool parseEndpoint(const char* text, ServerEndpoint& result) {
    while(*text==' ' || *text=='\t')++text;
    std::size_t size=std::strlen(text);
    while(size && (text[size-1]==' ' || text[size-1]=='\t' || text[size-1]=='\r' || text[size-1]=='\n'))--size;
    if(size>=7 && std::strncmp(text,"http://",7)==0){text+=7;size-=7;}
    if(size && text[size-1]=='/')--size;
    if(!size)return false;
    std::size_t hostSize=0;
    while(hostSize<size && text[hostSize]!=':')++hostSize;
    if(!hostSize || hostSize>=kEndpointHostCapacity)return false;
    unsigned port=80;
    if(hostSize<size){
        port=0;unsigned digits=0;
        for(std::size_t i=hostSize+1;i<size;++i){
            if(text[i]<'0' || text[i]>'9' || ++digits>5)return false;
            port=port*10+text[i]-'0';
        }
        if(!digits || !port || port>65535)return false;
    }
    ServerEndpoint parsed{};
    bool numeric=true;
    for(std::size_t i=0;i<hostSize;++i){
        char c=text[i];if((c<'0'||c>'9') && c!='.')numeric=false;
        parsed.host[i]=c>='A'&&c<='Z'?c+('a'-'A'):c;
    }
    if(numeric){
        unsigned octets[4]{};std::size_t at=0;
        for(unsigned part=0;part<4;++part){
            unsigned digits=0;
            while(at<hostSize && parsed.host[at]>='0' && parsed.host[at]<='9'){
                if(++digits>3)return false;
                octets[part]=octets[part]*10+parsed.host[at++]-'0';
            }
            if(!digits || octets[part]>255)return false;
            if(part<3 && (at==hostSize || parsed.host[at++]!='.'))return false;
        }
        if(at!=hostSize)return false;
        std::snprintf(parsed.host,sizeof(parsed.host),"%u.%u.%u.%u",octets[0],octets[1],octets[2],octets[3]);
    }else{
        // A final DNS root dot is optional; store a canonical hostname.
        if(parsed.host[hostSize-1]=='.')parsed.host[--hostSize]=0;
        if(!hostSize)return false;
        unsigned label=0;
        for(std::size_t i=0;i<hostSize;++i){
            char c=parsed.host[i];
            if(c=='.'){
                if(!label || parsed.host[i-1]=='-')return false;
                label=0;continue;
            }
            if(!((c>='a'&&c<='z') || (c>='0'&&c<='9') || c=='-'))return false;
            if((!label && c=='-') || ++label>63)return false;
        }
        if(!label || parsed.host[hostSize-1]=='-')return false;
    }
    parsed.port=port;result=parsed;return true;
}
void formatEndpoint(const ServerEndpoint& value, char* text, std::size_t capacity) {
    std::snprintf(text, capacity, "%s:%u", value.host, value.port);
}
const ServerEndpoint& currentEndpoint() { return profiles[emulator ? 1 : 0]; }
void setEndpoint(const ServerEndpoint& endpoint) { profiles[emulator ? 1 : 0] = endpoint; }
bool emulatorMode() { return emulator; }
void selectEmulatorMode(bool value) { emulator = value; }
