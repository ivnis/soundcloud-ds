#include "dns.h"
#include <nds.h>
#include <dswifi9.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace {
DnsCache cache;
cothread_t worker=-1;
char workerHost[kEndpointHostCapacity]{};
unsigned workerGeneration=0;
uint32_t workerAddress=0;
bool workerSuccess=false;

int resolve(void*){
    addrinfo hints{};hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;
    addrinfo* result=nullptr;
    workerSuccess=false;
    if(getaddrinfo(workerHost,nullptr,&hints,&result)==0){
        for(auto* item=result;item;item=item->ai_next){
            if(item->ai_family==AF_INET && item->ai_addr && item->ai_addrlen>=sizeof(sockaddr_in)){
                workerAddress=reinterpret_cast<sockaddr_in*>(item->ai_addr)->sin_addr.s_addr;
                workerSuccess=true;break;
            }
        }
    }
    if(result)freeaddrinfo(result);
    return 0;
}
void collect(unsigned frame){
    if(worker<0 || !cothread_has_joined(worker))return;
    cothread_delete(worker);worker=-1;
    cache.publish(workerHost,workerAddress,workerSuccess,frame,workerGeneration);
}
}

DnsStatus resolveServerIPv4(const char* host,unsigned frame,uint32_t& address){
    collect(frame);
    in_addr numeric{};
    if(inet_pton(AF_INET,host,&numeric)==1){address=numeric.s_addr;return DnsStatus::Ready;}
    DnsStatus state;
    if(cache.lookup(host,frame,address,state))return state;
    // One bounded resolver is shared by audio/search/artwork/settings. Never
    // destroy an in-flight lwIP DNS call; stale results are rejected by epoch.
    if(worker>=0)return DnsStatus::Pending;
    std::snprintf(workerHost,sizeof(workerHost),"%s",host);
    workerGeneration=cache.generation();workerAddress=0;workerSuccess=false;
    worker=cothread_create(resolve,nullptr,8192,0);
    if(worker<0){cache.publish(host,0,false,frame,workerGeneration);return DnsStatus::Error;}
    return DnsStatus::Pending;
}
void invalidateServerDns(){cache.invalidate();}
