#pragma once
#include "dns_cache.h"

// Addresses are IPv4 in network byte order, ready for sockaddr_in.
DnsStatus resolveServerIPv4(const char* host,unsigned frame,uint32_t& address);
void invalidateServerDns();
