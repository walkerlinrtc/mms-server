#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/awaitable.hpp>

#include <variant>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include "tls_socket.hpp"
#include "server/tls/tls_session.h"

using namespace mms;
TlsSocket::TlsSocket(SocketInterfaceHandler *handler, std::shared_ptr<TlsSession> tls_session) :
                                SocketInterface(handler),
                                connect_timeout_timer_(tls_session->get_tcp_socket()->get_worker()->get_io_context()),     
                                handshake_result_channel_(tls_session->get_tcp_socket()->get_worker()->get_io_context())
{
    encrypted_buf_ = new uint8_t[TLS_MAX_RECV_BUF];
    tls_session_ = tls_session;
}

TlsSocket::TlsSocket(SocketInterfaceHandler *handler, ThreadWorker *worker) : SocketInterface(handler),
                                        connect_timeout_timer_(worker->get_io_context()),     
                                        handshake_result_channel_(worker->get_io_context())
{
    auto tcp_socket = std::make_shared<TcpSocket>(this, boost::asio::ip::tcp::socket(worker->get_io_context()), worker);
    tls_session_ = std::make_shared<TlsSession>(false, handler, nullptr, tcp_socket);//客户端模式
}

std::shared_ptr<TlsSession> TlsSocket::get_tls_session() {
    return tls_session_;
}

TlsSocket::~TlsSocket() {
    spdlog::info("destroy TlsSocket");
    if (ssl_) {
        SSL_free(ssl_);
        ssl_ = nullptr;
    }

    if (encrypted_buf_) {
        delete[] encrypted_buf_;
        encrypted_buf_ = nullptr;
    }
    // bio_read_,bio_write_ 已经绑定了ssl_，所以不需要额外释放，否则内存异常
    if (ssl_ctx_) {
        SSL_CTX_free(ssl_ctx_);
        ssl_ctx_= nullptr;
    }

    if (encrypted_buf_) {
        delete[] encrypted_buf_;
        encrypted_buf_ = nullptr;
    }
}

ThreadWorker *TlsSocket::get_worker() {
    auto tcp_sock = tls_session_->get_tcp_socket();
    if (!tcp_sock) {
        return nullptr;
    }
    return tcp_sock->get_worker();
}

void TlsSocket::on_socket_open(std::shared_ptr<SocketInterface> tcp_sock) {
    if (tls_session_) {
        tcp_sock->set_session(tls_session_);
    }
}

void TlsSocket::on_socket_close(std::shared_ptr<SocketInterface> tcp_sock) {
    auto s = tcp_sock->get_session();
    if (s) {
        s->close();
    }
    close();
}

void TlsSocket::open() {
    if (handler_) {
        handler_->on_socket_open(shared_from_this());
    }
    return;
}

void TlsSocket::close() {
    spdlog::info("TlsSocket::close");
    if (closed_.test_and_set(std::memory_order_acquire)) {
        return;
    }

    if (tls_session_) {
        tls_session_->close();
        tls_session_ = nullptr;
    }

    if (handler_) {
        spdlog::info("handler_->on_socket_close(shared_from_this());");
        handler_->on_socket_close(shared_from_this());
    }

    return;
}

boost::asio::awaitable<bool> TlsSocket::connect(const std::string & ip, uint16_t port, int32_t timeout_ms) {
    ((void)timeout_ms);
    ssl_ctx_ = SSL_CTX_new(TLSv1_2_method());
    if (!ssl_ctx_) {
        co_return false;
    }

    SSL_CTX_set_verify(ssl_ctx_, SSL_VERIFY_NONE, NULL);

    ssl_ = SSL_new(ssl_ctx_);
    bio_read_  = BIO_new(BIO_s_mem());
    bio_write_ = BIO_new(BIO_s_mem());
    SSL_set_bio(ssl_, bio_read_, bio_write_);
    SSL_set_connect_state(ssl_);
    SSL_set_mode(ssl_, SSL_MODE_AUTO_RETRY);
    
    //启动tls握手协程
    auto tcp_sock = tls_session_->get_tcp_socket();
    if (!tcp_sock) {
        co_return false;
    }

    if (!co_await tcp_sock->connect(ip, port, timeout_ms)) {
        SSL_CTX_free(ssl_ctx_);
        ssl_ctx_= nullptr;
        co_return false;
    }
    
    auto self(shared_from_this());
    boost::asio::co_spawn(get_worker()->get_io_context(), [this, self, tcp_sock, timeout_ms]()->boost::asio::awaitable<void> {
        char* out_data = nullptr;
        int32_t result = 0;
        int ret;
        while (!SSL_is_init_finished(ssl_)) {
            ret = SSL_do_handshake(ssl_);
            if (1 == ret) {
                result = 0;//连接成功
                break;
            }

            int reason = SSL_get_error(ssl_, ret);
            if (reason != SSL_ERROR_WANT_READ && reason != SSL_ERROR_WANT_WRITE && reason != SSL_ERROR_NONE) {
                result = -1;
                break;
            }

            auto send_data_size = BIO_get_mem_data(bio_write_, &out_data);
            if (send_data_size > 0) {
                if (!co_await tcp_sock->send((uint8_t*)out_data, send_data_size)) {
                    result = -2;
                    break;
                }

                if (BIO_reset(bio_write_) != 1) {
                    result = -3;
                    break;
                }
            }

            if (reason == SSL_ERROR_WANT_READ) {
                int32_t len = co_await tcp_sock->recv_some(encrypted_buf_, TLS_MAX_RECV_BUF, timeout_ms);
                if (len < 0) {
                    result = -4;
                    break;
                }

                int ret = BIO_write(bio_read_, encrypted_buf_, len);
                if (ret <= 0) {
                    result = -5;
                    break;
                }
            }
        }
        
        co_await handshake_result_channel_.async_send(boost::system::error_code{}, result, boost::asio::use_awaitable);
        co_return;
    }, boost::asio::detached);

    boost::system::error_code ec;
    int res = 0;
    res = co_await handshake_result_channel_.async_receive(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    if (0 != res || ec) {
        co_return false;
    }
    co_return true;
}

int TlsSocket::on_tls_ext_servername_cb(SSL *ssl, int *ad, void *arg) {
    TlsSocket *this_ptr = (TlsSocket*)arg;
    return this_ptr->do_on_tls_ext_servername_cb(ssl, ad, arg);
}

int TlsSocket::do_on_tls_ext_servername_cb(SSL *ssl, int *ad, void *arg) {
    ((void)ad);
    ((void)arg);
    if(ssl == NULL) {
        return SSL_TLSEXT_ERR_NOACK;
    }

    const char * servername = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
    if (servername == NULL) {
        return SSL_TLSEXT_ERR_NOACK;
    }

    std::string domain(servername);
    if (!server_name_handler_) {
        return SSL_TLSEXT_ERR_NOACK;
    }

    auto cert_ctx = server_name_handler_->on_tls_ext_servername(domain);
    if (!cert_ctx) {
        return SSL_TLSEXT_ERR_NOACK;
    }

    use_ssl_ctx_ = cert_ctx;
    SSL_set_SSL_CTX(ssl, cert_ctx.get());
    return SSL_TLSEXT_ERR_OK;
}

boost::asio::awaitable<bool> TlsSocket::do_handshake(int32_t timeout_ms) {
    ssl_ctx_ = SSL_CTX_new(TLSv1_2_method());
    if (!ssl_ctx_) {
        co_return false;
    }

    auto tcp_sock = tls_session_->get_tcp_socket();
    if (!tcp_sock) {
        spdlog::error("no tls session");
        co_return false;
    }
    
    SSL_CTX_set_verify(ssl_ctx_, SSL_VERIFY_NONE, NULL);
    SSL_CTX_set_tlsext_servername_arg(ssl_ctx_, this);
    SSL_CTX_set_tlsext_servername_callback(ssl_ctx_, on_tls_ext_servername_cb);

    ssl_ = SSL_new(ssl_ctx_);
    bio_read_  = BIO_new(BIO_s_mem());
    bio_write_ = BIO_new(BIO_s_mem());

    SSL_set_bio(ssl_, bio_read_, bio_write_);
    SSL_set_accept_state(ssl_);
    SSL_set_mode(ssl_, SSL_MODE_ENABLE_PARTIAL_WRITE);
    auto self(shared_from_this());
    //启动tls握手协程
    boost::asio::co_spawn(get_worker()->get_io_context(), [this, self, tcp_sock, timeout_ms]()->boost::asio::awaitable<void> {
        char* out_data = nullptr;
        int32_t result = 0;
        
        while (true) {
            int32_t len = co_await tcp_sock->recv_some(encrypted_buf_, TLS_MAX_RECV_BUF, timeout_ms);
            if (len < 0) {
                result = -1;
                break;
            }

            int ret = BIO_write(bio_read_, encrypted_buf_, len);
            if (ret <= 0) {
                result = -2;
                break;
            }

            ret = SSL_do_handshake(ssl_);
            if (ret == 1) {
                // 连接成功
                int r1 = SSL_get_error(ssl_, ret);
                if (r1 != SSL_ERROR_WANT_READ && r1 != SSL_ERROR_WANT_WRITE && r1 != SSL_ERROR_NONE) {
                    result = -3;
                    break;
                }
                // 判断是否有数据要发送
                auto send_data_size = BIO_get_mem_data(bio_write_, &out_data);
                if (send_data_size > 0) {
                    co_await tcp_sock->send((uint8_t*)out_data, send_data_size, timeout_ms);
                    if (BIO_reset(bio_write_) != 1) {
                        result = -4;
                        break;
                    }
                }
                break;
            } 
            // 失败了，但如果是READ/WRITE则不算失败
            int r1 = SSL_get_error(ssl_, ret);
            if (r1 != SSL_ERROR_WANT_READ && r1 != SSL_ERROR_WANT_WRITE && r1 != SSL_ERROR_NONE) {
                result = -5;
                break;
            }
            // 判断是否有数据要发送
            auto send_data_size = BIO_get_mem_data(bio_write_, &out_data);
            if (send_data_size > 0) {
                co_await tcp_sock->send((uint8_t*)out_data, send_data_size, timeout_ms);
                if (BIO_reset(bio_write_) != 1) {
                    result = -6;
                    break;
                }
            }
        }
        co_await handshake_result_channel_.async_send(boost::system::error_code{}, result, boost::asio::use_awaitable);
        co_return;
    }, boost::asio::detached);
    
    boost::system::error_code ec;
    auto res = co_await handshake_result_channel_.async_receive(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    if (res == 0) {
        co_return true;
    } else {
        co_return false;
    }
}

boost::asio::awaitable<int32_t> TlsSocket::recv_some(uint8_t *data, size_t len, int32_t timeout_ms)
{
    auto tcp_sock = tls_session_->get_tcp_socket();
    if (!tcp_sock) {
        co_return -1;
    }

    while (true) {
        int r0 = SSL_read(ssl_, data, len);
        if (r0 == 0) {//对端关闭连接
            co_return -1;
        } else if (r0 > 0) {//大于0，是字节数
            co_return r0;
        }

        int r1 = SSL_get_error(ssl_, r0);
        if (r1 == SSL_ERROR_WANT_READ) {
            auto len = co_await tcp_sock->recv_some(encrypted_buf_, TLS_MAX_RECV_BUF, timeout_ms);
            if (len < 0) {
                co_return -2;
            }

            int ret;
            if ((ret = BIO_write(bio_read_, encrypted_buf_, len)) <= 0) {
                co_return -3;
            }
        }
    }
    co_return -4;
}

boost::asio::awaitable<bool> TlsSocket::send(const uint8_t *data, size_t len, int32_t timeout_ms)
{
    auto tcp_sock = tls_session_->get_tcp_socket();
    if (!tcp_sock) {
        co_return false;
    }

    size_t data_pos = 0;
    char* out_data = nullptr;
    int send_data_size;
    while (data_pos < len) {
        int r0 = SSL_write(ssl_, data + data_pos, len - data_pos);
        if (r0 == 0) {
            co_return false;
        } else if (r0 > 0) {
            data_pos += r0;
        }

        int r1 = SSL_get_error(ssl_, r0);
        if (r1 != SSL_ERROR_NONE && r1 != SSL_ERROR_WANT_WRITE) {
            co_return false;
        }

        // 判断是否有数据要发送
        send_data_size = BIO_get_mem_data(bio_write_, &out_data);
        if (send_data_size > 0) {
            if (!co_await tcp_sock->send((uint8_t*)out_data, send_data_size, timeout_ms)) {
                co_return false;
            }

            if (BIO_reset(bio_write_) != 1) {
                co_return false;
            }
        }
    }
    co_return true;
}

boost::asio::awaitable<bool> TlsSocket::send(std::vector<boost::asio::const_buffer> & bufs, int32_t timeout_ms) {
    auto tcp_sock = tls_session_->get_tcp_socket();
    if (!tcp_sock) {
        co_return false;
    }

    char* out_data = nullptr;
    int send_data_size;
    int32_t curr_buf_index = 0;
    size_t curr_buf_offset = 0;
    int32_t len = 0;
    for (auto & buf : bufs) {
        len += buf.size();
    }
    int32_t sended_bytes = 0;
    int r0;
    while (sended_bytes < len) {
        while (sended_bytes < len) {
            r0 = SSL_write(ssl_, (char*)(bufs[curr_buf_index].data()) + curr_buf_offset, bufs[curr_buf_index].size() - curr_buf_offset);
            if (r0 <= 0) {
                int r1 = SSL_get_error(ssl_, r0);
                if (r1 != SSL_ERROR_NONE && r1 != SSL_ERROR_WANT_WRITE) {
                    co_return false;
                }
                break;
            } else if (r0 > 0) {
                sended_bytes += r0;
                curr_buf_offset += r0;
                if (curr_buf_offset >= bufs[curr_buf_index].size()) {
                    curr_buf_index++;
                    curr_buf_offset = 0;
                }
            }
        }
        
        int r1 = SSL_get_error(ssl_, r0);
        if (r1 != SSL_ERROR_NONE && r1 != SSL_ERROR_WANT_WRITE) {
            co_return false;
        }

        // 判断是否有数据要发送
        send_data_size = BIO_get_mem_data(bio_write_, &out_data);
        if (send_data_size > 0) {
            if (!co_await tcp_sock->send((uint8_t*)out_data, send_data_size, timeout_ms)) {
                co_return false;
            }

            if (BIO_reset(bio_write_) != 1) {
                co_return false;
            }
        }
    }
    co_return true;
}

boost::asio::awaitable<bool> TlsSocket::recv(uint8_t *data, size_t len, int32_t timeout_ms)
{
    auto tcp_sock = tls_session_->get_tcp_socket();
    if (!tcp_sock) {
        co_return false;
    }

    size_t data_pos = 0;
    while (data_pos < len) {
        int r0 = SSL_read(ssl_, data + data_pos, len - data_pos);
        if (r0 == 0) {//对端关闭连接
            co_return false;
        } else if (r0 > 0) {//大于0，是字节数
            data_pos += r0;
            continue;
        }

        int r1 = SSL_get_error(ssl_, r0);
        if (r1 == SSL_ERROR_WANT_READ) {
            auto len = co_await tcp_sock->recv_some(encrypted_buf_, TLS_MAX_RECV_BUF, timeout_ms);
            if (len < 0) {
                co_return false;
            }

            int ret;
            if ((ret = BIO_write(bio_read_, encrypted_buf_, len)) <= 0) {
                co_return false;
            }
        }
    }
    co_return true;
}
