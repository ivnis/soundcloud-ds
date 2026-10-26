#pragma once
#include "http.h"
#include "endpoint.h"

class NetRequest {
public:
    enum class State { Idle, Resolving, Connecting, Sending, Receiving, Done, Error };
    explicit NetRequest(bool pcm) : response_(pcm), pcm_(pcm) {}
    bool begin(const char* path, unsigned frame);
    void cancel();
    void poll(unsigned frame, unsigned budget, HttpAudio::Sink sink, void* context);
    State state() const { return state_; }
    bool active() const { return state_ == State::Resolving || state_ == State::Connecting || state_ == State::Sending || state_ == State::Receiving; }
    const HttpAudio& response() const { return response_; }
    const char* error() const { return error_; }
    bool recoverable() const { return recoverable_; }
private:
    HttpAudio response_;
    bool pcm_;
    bool recoverable_ = false;
    int socket_ = -1;
    State state_ = State::Idle;
    unsigned lastActivity_ = 0;
    char request_[1536]{};
    ServerEndpoint endpoint_{};
    unsigned sent_ = 0, length_ = 0;
    const char* error_ = "";
    void close();
    void fail(const char* message, bool recoverable = false);
    void finish();
};
