#pragma once
#include <memory>

#include "base/network/tls_socket.hpp"
#include "server/tcp/tcp_server.hpp"
#include "server/tls/tls_session.h"

namespace mms {
class TlsServer : public TcpServer, public SocketInterfaceHandler {
public:
    TlsServer(SocketInterfaceHandler *tls_handler, TlsServerNameHandler * name_handler,ThreadWorker *worker);
    virtual ~TlsServer();
public:
    int32_t start_listen(uint16_t port);
    void stop_listen();
private:
    void on_socket_open(std::shared_ptr<SocketInterface> tcp_socket) final override;
    void on_socket_close(std::shared_ptr<SocketInterface> tcp_socket) final override;
private:
    bool init_ssl();
    SocketInterfaceHandler *tls_handler_ = nullptr;
    TlsServerNameHandler *server_name_handler_ = nullptr;
};
};