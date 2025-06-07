#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include "tls_session.h"
#include "base/network/tls_socket.hpp"

using namespace mms;
TlsSession::TlsSession(bool mode, SocketInterfaceHandler *tls_handler, TlsServerNameHandler *server_name_handler, std::shared_ptr<TcpSocket> tcp_socket) :
                                                            Session(tcp_socket->get_worker()), 
                                                            tls_socket_handler_(tls_handler), 
                                                            server_name_handler_(server_name_handler), 
                                                            tcp_socket_(tcp_socket) {
    is_server_mode_ = mode;
}

TlsSession::~TlsSession() {
    spdlog::debug("destroy TlsSession");
}

std::shared_ptr<TcpSocket> TlsSession::get_tcp_socket() {
    return tcp_socket_;
}

void TlsSession::service() {
    auto self(shared_from_this());
    boost::asio::co_spawn(get_worker()->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        tls_socket_ = std::make_shared<TlsSocket>(tls_socket_handler_, std::static_pointer_cast<TlsSession>(self));
        if (is_server_mode_) {
            tls_socket_->set_cert_handler(server_name_handler_);
            bool ret = co_await tls_socket_->do_handshake();
            if (!ret) {
                close();
                co_return;
            }

            tls_socket_->open();
        }
    }, boost::asio::detached);
}

void TlsSession::close() {
    if (closed_.test_and_set()) {
        return;
    }

    auto self(this->shared_from_this());
    boost::asio::co_spawn(get_worker()->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        boost::system::error_code ec;
        if (tls_socket_) {
            tls_socket_->close();
            tls_socket_ = nullptr;
        }

        if (tcp_socket_) {
            tcp_socket_->close();
            tcp_socket_ = nullptr;
        }

        co_return;
    }, boost::asio::detached);
}