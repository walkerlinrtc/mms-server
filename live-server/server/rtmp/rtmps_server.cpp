#include <memory>

#include "log/log.h"
#include "rtmps_server.hpp"
#include "rtmp_server_session.hpp"
#include "config/config.h"

using namespace mms;

RtmpsServer::RtmpsServer(ThreadWorker *w) {
    tls_server_ = std::make_unique<TlsServer>(this, this, w);
}

RtmpsServer::~RtmpsServer() {

}

bool RtmpsServer::start(uint16_t port) {
    tls_server_->set_socket_inactive_timeout_ms(Config::get_instance()->get_socket_inactive_timeout_ms());
    if (0 == tls_server_->start_listen(port)) {
        return true;
    }
    return false;
}

void RtmpsServer::stop() {
    tls_server_->stop_listen();
}

void RtmpsServer::on_socket_open(std::shared_ptr<SocketInterface> tls_socket) {
    std::shared_ptr<RtmpServerSession> s = std::make_shared<RtmpServerSession>(tls_socket);
    tls_socket->set_session(s);
    s->service();
}

void RtmpsServer::on_socket_close(std::shared_ptr<SocketInterface> tls_socket) {
    std::shared_ptr<RtmpServerSession> s = std::static_pointer_cast<RtmpServerSession>(tls_socket->get_session());
    tls_socket->clear_session();
    if (s) {
        s->close();
    }
}

std::shared_ptr<SSL_CTX> RtmpsServer::on_tls_ext_servername(const std::string & domain_name) {
    auto c = Config::get_instance();
    if (!c) {
        CORE_ERROR("could not find cert for:{}", domain_name);
        return nullptr;
    }
    return c->get_cert_manager().get_cert_ctx(domain_name);
}