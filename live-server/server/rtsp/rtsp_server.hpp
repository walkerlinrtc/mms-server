#pragma once
#include <memory>
#include <functional>

#include "server/tcp/tcp_server.hpp"
#include "server/udp/udp_server.hpp"

namespace mms {
class RtspServer : public UdpServer, public TcpServer, public SocketInterfaceHandler {
public:
public:
    RtspServer(const std::vector<ThreadWorker*> & workers);
    virtual ~RtspServer();
    
    bool start(uint16_t port = 554);
    void stop();
    boost::asio::awaitable<void> on_udp_socket_recv(UdpSocket *sock, std::unique_ptr<uint8_t[]> data, size_t len, boost::asio::ip::udp::endpoint &remote_ep) override;
private:
    void on_socket_open(std::shared_ptr<SocketInterface> tcp_socket) override;
    void on_socket_close(std::shared_ptr<SocketInterface> tcp_socket) override;
};
};