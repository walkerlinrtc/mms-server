#pragma once
#include <memory>
#include <functional>

#include "server/tcp/tcp_server.hpp"
#include "server/tls/tls_server.hpp"
#include "server/udp/udp_server.hpp"

#include "base/thread/thread_pool.hpp"
#include "protocol/http/http_define.h"

namespace mms {
class RtspsServer : public UdpServer, public SocketInterfaceHandler, public TlsServerNameHandler {
public:
    RtspsServer(const std::vector<ThreadWorker*> & workers);
    virtual ~RtspsServer();
    
    bool start(uint16_t port = 8554);
    void stop();
    boost::asio::awaitable<void> on_udp_socket_recv(UdpSocket *sock, std::unique_ptr<uint8_t[]> data, size_t len, boost::asio::ip::udp::endpoint &remote_ep) override;
private:
    std::unique_ptr<TlsServer> tls_server_;
    void on_socket_open(std::shared_ptr<SocketInterface> tls_socket) override;
    void on_socket_close(std::shared_ptr<SocketInterface> tls_socket) override;

    std::shared_ptr<SSL_CTX> on_tls_ext_servername(const std::string & domain_name) override;
};
};