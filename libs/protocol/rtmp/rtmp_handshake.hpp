#pragma once
#include "base/network/tcp_socket.hpp"

namespace mms {
class RtmpHandshake {
public:
    RtmpHandshake(std::shared_ptr<SocketInterface> sock);
    virtual ~RtmpHandshake();

    boost::asio::awaitable<bool> do_server_handshake();
    boost::asio::awaitable<bool> do_client_handshake();
private:
    void _genS0S1S2(uint8_t *c0c1, uint8_t *s0s1s2);
private:
    std::shared_ptr<SocketInterface> conn_;
};
};