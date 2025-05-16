#include <memory>

#include "rtmp_server.hpp"
#include "rtmp_server_session.hpp"
#include "config/config.h"
using namespace mms;

RtmpServer::RtmpServer(ThreadWorker *w):TcpServer(this, w) {
}

RtmpServer::~RtmpServer() {

}

bool RtmpServer::start(uint16_t port) {
    set_socket_inactive_timeout_ms(Config::get_instance()->get_socket_inactive_timeout_ms());
    if (0 == start_listen(port)) {
        return true;
    }
    return false;
}

void RtmpServer::stop() {
    stop_listen();
}

void RtmpServer::on_socket_open(std::shared_ptr<SocketInterface> tcp_socket) {
    std::shared_ptr<RtmpServerSession> s = std::make_shared<RtmpServerSession>(tcp_socket);
    tcp_socket->set_session(s);
    s->service();
}

void RtmpServer::on_socket_close(std::shared_ptr<SocketInterface> tcp_socket) {
    std::shared_ptr<RtmpServerSession> s = std::static_pointer_cast<RtmpServerSession>(tcp_socket->get_session());
    tcp_socket->clear_session();
    if (s) {
        s->close();
    }
}