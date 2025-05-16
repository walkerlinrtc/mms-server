#pragma once

#include <stdint.h>
#include <string>
#include <iostream>
#include <memory>
#include <vector>

#include <boost/array.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/spawn.hpp>

#include "base/network/udp_socket.hpp"
namespace mms {
#define MAX_UDP_RECV_BUF 2*1024*1024
class ThreadWorker;
class UdpSocket;

class UdpServer : public UdpSocketHandler {
public:
    UdpServer(const std::vector<ThreadWorker*> & workers);
    virtual ~UdpServer();
    bool start_listen(const std::string & ip, uint16_t port);
    void stop_listen();
protected:
    uint16_t port_;
    std::vector<ThreadWorker*> workers_;
    std::vector<UdpSocket*> udp_socks_;
    std::vector<boost::array<uint8_t, MAX_UDP_RECV_BUF>> recv_bufs_;
    bool running_ = false;
};
};