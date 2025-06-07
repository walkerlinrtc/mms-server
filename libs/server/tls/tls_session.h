#pragma once
#include <memory>

#include "server/session.hpp"
namespace mms {
class TcpSocket;
class TlsSocketHandler;
class TlsServerNameHandler;
class SocketInterfaceHandler;
class TlsSocket;

class TlsSession : public Session {
public:
    TlsSession(bool mode, SocketInterfaceHandler *tls_handler, TlsServerNameHandler *server_name_handler, std::shared_ptr<TcpSocket> tcp_socket);
    virtual ~TlsSession();
    std::shared_ptr<TcpSocket> get_tcp_socket();

    void service() override;
    void close() override;
protected:
    SocketInterfaceHandler *tls_socket_handler_ = nullptr;
    TlsServerNameHandler * server_name_handler_ = nullptr;
    std::shared_ptr<TcpSocket> tcp_socket_;
    std::shared_ptr<TlsSocket> tls_socket_;
    bool is_server_mode_;
};
};