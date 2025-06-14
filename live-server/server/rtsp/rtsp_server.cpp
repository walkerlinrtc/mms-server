#include <boost/shared_ptr.hpp>
#include <memory>

#include "log/log.h"
#include "rtsp_server.hpp"
#include "rtsp_server_session.hpp"
#include "config/config.h"

namespace mms {

RtspServer::RtspServer(const std::vector<ThreadWorker*> & workers):UdpServer(workers), TcpServer(this, workers[0]) {
}

RtspServer::~RtspServer() {

}

bool RtspServer::start(uint16_t port) {
    set_socket_inactive_timeout_ms(Config::get_instance()->get_socket_inactive_timeout_ms());
    if (0 == TcpServer::start_listen(port)) {
        CORE_INFO("listen rtsp port:{} succeed", port);
        return true;
    } 
    return false;
}

void RtspServer::stop() {
    TcpServer::stop_listen();
}

void RtspServer::on_socket_open(std::shared_ptr<SocketInterface> tcp_socket) {
    std::shared_ptr<RtspServerSession> s = std::make_shared<RtspServerSession>(tcp_socket);
    tcp_socket->set_session(s);
    s->start();
}

void RtspServer::on_socket_close(std::shared_ptr<SocketInterface> tcp_socket) {
    std::shared_ptr<RtspServerSession> s = std::static_pointer_cast<RtspServerSession>(tcp_socket->get_session());
    tcp_socket->clear_session();
    if (s) {
        s->stop();
    }
}

boost::asio::awaitable<void> RtspServer::on_udp_socket_recv(UdpSocket *sock, std::unique_ptr<uint8_t[]> data, size_t len, boost::asio::ip::udp::endpoint &remote_ep)
{
    (void)sock;
    (void)data;
    (void)len;
    (void)remote_ep;
    co_return;
}
};