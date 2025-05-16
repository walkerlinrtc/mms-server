#include "openssl/ssl.h"
#include "openssl/err.h"

#include "tls_server.hpp"
using namespace mms;
TlsServer::TlsServer(SocketInterfaceHandler *tls_handler, TlsServerNameHandler * name_handler,ThreadWorker *worker) : TcpServer(this, worker), tls_handler_(tls_handler), server_name_handler_(name_handler) {

}

TlsServer::~TlsServer() {
}

int32_t TlsServer::start_listen(uint16_t port) {
    if (!init_ssl()) {
        spdlog::error("init ssl failed");
        return -1;
    }

    return TcpServer::start_listen(port);
}

void TlsServer::stop_listen() {
    TcpServer::stop_listen();
}

void TlsServer::on_socket_open(std::shared_ptr<SocketInterface> tcp_socket) {
    std::shared_ptr<TlsSession> tls_session = std::make_shared<TlsSession>(true, tls_handler_, server_name_handler_, std::static_pointer_cast<TcpSocket>(tcp_socket));
    tcp_socket->set_session(tls_session);
    tls_session->service();
}

void TlsServer::on_socket_close(std::shared_ptr<SocketInterface> tcp_socket) {
    auto s = tcp_socket->get_session();
    tcp_socket->clear_session();
    if (!s) {
        return;
    }
    s->close();
}

bool TlsServer::init_ssl() {
    OpenSSL_add_all_algorithms();
    SSL_library_init();
    SSL_load_error_strings();
    return true;
}