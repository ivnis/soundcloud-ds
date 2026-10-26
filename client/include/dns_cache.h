#pragma once
#include "endpoint.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

enum class DnsStatus { Pending, Ready, Error };
class DnsCache {
public:
    unsigned generation() const {return generation_;}
    void invalidate(){++generation_;host_[0]=0;}
    bool lookup(const char* host,unsigned frame,uint32_t& address,DnsStatus& status) const {
        if(!host_[0] || std::strcmp(host,host_)!=0 || frame-created_>=lifetime_)return false;
        address=address_;status=status_;return true;
    }
    bool publish(const char* host,uint32_t address,bool success,unsigned frame,unsigned generation){
        if(generation!=generation_)return false;
        std::snprintf(host_,sizeof(host_),"%s",host);address_=address;created_=frame;
        status_=success?DnsStatus::Ready:DnsStatus::Error;lifetime_=success?60*300:60*2;
        return true;
    }
private:
    char host_[kEndpointHostCapacity]{};
    unsigned generation_=0,created_=0,lifetime_=0;
    uint32_t address_=0;
    DnsStatus status_=DnsStatus::Pending;
};
