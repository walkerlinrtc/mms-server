#pragma once
#include <array>
#include <boost/array.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/shared_ptr.hpp>
#include <memory>
#include <unordered_map>
#include <vector>

#include "base/network/tcp_socket.hpp"
#include "base/thread/thread_worker.hpp"
#include "openssl/bio.h"
#include "openssl/ssl.h"
#include "tcp_socket.hpp"


namespace mms {
#define TLS_MAX_RECV_BUF (2 * 1024 * 1024)
class TlsSocket;
class TlsSession;

class TlsServerNameHandler {
public:
    virtual std::shared_ptr<SSL_CTX> on_tls_ext_servername(const std::string &doamin_name) = 0;
};

class TlsSocket : public SocketInterface, public SocketInterfaceHandler {
public:
    TlsSocket(SocketInterfaceHandler *tls_handler, std::shared_ptr<TlsSession> tls_session);
    TlsSocket(SocketInterfaceHandler *tls_handler, ThreadWorker *worker);
    virtual ~TlsSocket();

    void set_cert_handler(TlsServerNameHandler *server_name_handler) {
        server_name_handler_ = server_name_handler;
    }

    ThreadWorker *get_worker() override;
    std::shared_ptr<TlsSession> get_tls_session();
    void open() override;
    void close() override;

    boost::asio::awaitable<bool> connect(const std::string &ip, uint16_t port,
                                         int32_t timeout_ms = 0) override;
    boost::asio::awaitable<int32_t> recv_some(uint8_t *data, size_t len, int32_t timeout_ms = 0) override;
    boost::asio::awaitable<bool> send(const uint8_t *data, size_t len, int32_t timeout_ms = 0) override;
    boost::asio::awaitable<bool> send(std::vector<boost::asio::const_buffer> &bufs,
                                      int32_t timeout_ms = 0) override;
    boost::asio::awaitable<bool> recv(uint8_t *data, size_t len, int32_t timeout_ms = 0) override;
    boost::asio::awaitable<bool> do_handshake(int32_t timeout_ms = 1000);

private:
    void on_socket_open(std::shared_ptr<SocketInterface> tcp_sock) override;
    void on_socket_close(std::shared_ptr<SocketInterface> tcp_sock) override;

    boost::asio::steady_timer connect_timeout_timer_;
    static int on_tls_ext_servername_cb(SSL *ssl, int *ad, void *arg);
    int do_on_tls_ext_servername_cb(SSL *ssl, int *ad, void *arg);
    boost::asio::experimental::concurrent_channel<void(boost::system::error_code, int32_t)>
        handshake_result_channel_;
    SSL *ssl_ = nullptr;
    BIO *bio_read_ = nullptr;
    BIO *bio_write_ = nullptr;

    uint8_t *encrypted_buf_ = nullptr;
    SSL_CTX *ssl_ctx_ = nullptr;
    std::shared_ptr<SSL_CTX> use_ssl_ctx_;

private:
    TlsServerNameHandler *server_name_handler_ = nullptr;
    std::shared_ptr<TlsSession> tls_session_;
};
};  // namespace mms