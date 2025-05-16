#include <boost/shared_ptr.hpp>
#include <memory>

#include "log/log.h"
#include "config/config.h"
#include "rtsps_server.hpp"
#include "rtsp_server_session.hpp"

namespace mms {

RtspsServer::RtspsServer(const std::vector<ThreadWorker*> & workers):UdpServer(workers) {
    tls_server_ = std::make_unique<TlsServer>(this, this, workers[0]);
}

RtspsServer::~RtspsServer() {

}

bool RtspsServer::start(uint16_t port) {
    tls_server_->set_socket_inactive_timeout_ms(Config::get_instance()->get_socket_inactive_timeout_ms());
    if (0 == tls_server_->start_listen(port)) {
        CORE_INFO("listen rtsps port:{} succeed", port);
        return true;
    } 
    return false;
}

void RtspsServer::stop() {
    tls_server_->stop_listen();
}

void RtspsServer::on_socket_open(std::shared_ptr<SocketInterface> tls_socket) {
    std::shared_ptr<RtspServerSession> s = std::make_shared<RtspServerSession>(tls_socket);
    tls_socket->set_session(s);
    s->service();
}

void RtspsServer::on_socket_close(std::shared_ptr<SocketInterface> tls_socket) {
    std::shared_ptr<RtspServerSession> s = std::static_pointer_cast<RtspServerSession>(tls_socket->get_session());
    tls_socket->clear_session();
    if (s) {
        s->close();
    }
}

std::shared_ptr<SSL_CTX> RtspsServer::on_tls_ext_servername(const std::string & domain_name) {
    auto c = Config::get_instance();
    if (!c) {
        CORE_ERROR("could not find cert for:{}", domain_name);
        return nullptr;
    }
    return c->get_cert_manager().get_cert_ctx(domain_name);
}

boost::asio::awaitable<void> RtspsServer::on_udp_socket_recv(UdpSocket *sock, std::unique_ptr<uint8_t[]> data, size_t len, boost::asio::ip::udp::endpoint &remote_ep)
{
    (void)sock;
    (void)data;
    (void)len;
    (void)remote_ep;
    co_return;
}
};