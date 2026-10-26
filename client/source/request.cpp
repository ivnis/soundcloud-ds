#include "request.h"
#include "config.h"
#include "endpoint.h"
#include "dns.h"
#include <nds.h>
#include <dswifi9.h>
#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

static bool waiting() { return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS || errno == EALREADY; }

void NetRequest::close() {
    if (socket_ >= 0) { shutdown(socket_, SHUT_RDWR); ::close(socket_); socket_ = -1; }
}
void NetRequest::cancel() { close(); state_ = State::Idle; }
void NetRequest::fail(const char* message) { close(); error_ = message; state_ = State::Error; }
void NetRequest::finish() {
    if (!response_.eof()) { fail(response_.error()); return; }
    close(); state_ = State::Done;
}

bool NetRequest::begin(const char* path, unsigned frame) {
    cancel();
    response_ = HttpAudio(pcm_);
    error_ = "";
    sent_ = 0;
    endpoint_=currentEndpoint();
    const auto& endpoint = endpoint_;
    int size = std::snprintf(request_, sizeof(request_),
        "GET %s HTTP/1.0\r\nHost: %s:%u\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n",
        path, endpoint.host, endpoint.port);
    if (size < 0 || static_cast<unsigned>(size) >= sizeof(request_)) { fail("Search query is too long"); return false; }
    length_ = size;
    state_=State::Resolving;
    lastActivity_=frame;
    return true;
}

void NetRequest::poll(unsigned frame, unsigned budget, HttpAudio::Sink sink, void* context) {
    if (!active()) return;
    if(state_==State::Resolving){
        uint32_t resolved=0;
        auto status=resolveServerIPv4(endpoint_.host,frame,resolved);
        if(status==DnsStatus::Error){fail("DNS failed. Check host and Wi-Fi.");return;}
        if(status==DnsStatus::Ready){
            sockaddr_in address{};address.sin_family=AF_INET;
            address.sin_port=htons(endpoint_.port);address.sin_addr.s_addr=resolved;
            socket_=socket(AF_INET,SOCK_STREAM,0);
            if(socket_<0){fail("Could not open connection");return;}
            int enabled=1;
            if(ioctl(socket_,FIONBIO,&enabled)<0){fail("Network setup failed");return;}
            int result=connect(socket_,reinterpret_cast<sockaddr*>(&address),sizeof(address));
            if(result<0 && !waiting()){invalidateServerDns();fail("Server is unreachable");return;}
            state_=result==0?State::Sending:State::Connecting;lastActivity_=frame;
        }
    }
    if (state_ == State::Connecting) {
        fd_set writes, errors;
        FD_ZERO(&writes); FD_ZERO(&errors);
        FD_SET(socket_, &writes); FD_SET(socket_, &errors);
        timeval timeout{};
        int ready = select(socket_ + 1, nullptr, &writes, &errors, &timeout);
        if (ready < 0) { fail("Connection failed"); return; }
        if (ready > 0) {
            int error = 0;
            socklen_t size = sizeof(error);
            if (getsockopt(socket_, SOL_SOCKET, SO_ERROR, &error, &size) < 0 || error) { invalidateServerDns(); fail("Server is unavailable"); return; }
            state_ = State::Sending;
            lastActivity_ = frame;
        }
    }
    if (state_ == State::Sending) {
        int sent = send(socket_, request_ + sent_, length_ - sent_, 0);
        if (sent > 0) { sent_ += sent; lastActivity_ = frame; }
        else if (sent < 0 && !waiting()) { fail("Request could not be sent"); return; }
        if (sent_ == length_) state_ = State::Receiving;
    }
    if (state_ == State::Receiving) {
        if (!budget) { lastActivity_ = frame; return; }
        for (int pass = 0; pass < 8 && budget; ++pass) {
            uint8_t data[2048];
            unsigned amount = budget < sizeof(data) ? budget : sizeof(data);
            int count = recv(socket_, data, amount, 0);
            if (count > 0) {
                budget -= count;
                lastActivity_ = frame;
                if (!response_.feed(data, count, sink, context)) { fail(response_.error()); return; }
                if (response_.done()) { finish(); return; }
            } else if (count == 0) { finish(); return; }
            else { if (!waiting()) fail("Connection was lost"); break; }
        }
    }
    if (active() && frame - lastActivity_ > 60 * (pcm_ ? 65u : 40u)) fail("Server timed out. Try again.");
}
