/*
 * @Author: jbl19860422
 * @Date: 2023-08-26 22:56:09
 * @LastEditTime: 2023-12-03 11:15:45
 * @LastEditors: jbl19860422
 * @Description:
 * @FilePath: \mms\mms\server\webrtc\dtls_boringssl\dtls_boringssl_session.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved.
 */
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <variant>

using namespace boost::asio::experimental::awaitable_operators;

#include "../webrtc_server_session.hpp"
#include "base/thread/thread_worker.hpp"
#include "base/utils/utils.h"
#include "boringssl_session.h"
#include "config/config.h"
#include "dtls_cert.h"
#include "spdlog/spdlog.h"


using namespace mms;
#define TLS_MAX_RECV_BUF 1024 * 1024

DtlsBoringSSLSession::DtlsBoringSSLSession(ThreadWorker *worker, WebRtcServerSession &session)
    : worker_(worker),
      timeout_timer_(worker->get_io_context()),
      session_(session),
      dtls_state_channel_(worker->get_io_context()),
      handshaking_coroutine_exited_(worker->get_io_context()) {
    encrypted_buf_ = new uint8_t[TLS_MAX_RECV_BUF];
}

DtlsBoringSSLSession::~DtlsBoringSSLSession() {
    spdlog::debug("destroy DtlsBoringSSLSession");
    if (ssl_) {
        SSL_free(ssl_);
        ssl_ = nullptr;
    }

    // bio_read_,bio_write_ 已经绑定了ssl_，所以不需要额外释放，否则内存异常
    if (ssl_ctx_) {
        SSL_CTX_free(ssl_ctx_);
        ssl_ctx_ = nullptr;
    }

    if (encrypted_buf_) {
        delete[] encrypted_buf_;
        encrypted_buf_ = nullptr;
    }
}

int DtlsBoringSSLSession::on_ssl_certificate_verify(int /*preverifyOk*/, X509_STORE_CTX * /*ctx*/) {
    // Always valid since DTLS certificates are self-signed.
    return 1;
}

void DtlsBoringSSLSession::on_ssl_info(const SSL *ssl, int where, int ret) {
    DtlsBoringSSLSession *this_ptr = (DtlsBoringSSLSession *)SSL_get_ex_data(ssl, 0);
    this_ptr->do_on_ssl_info(ssl, where, ret);
}

void DtlsBoringSSLSession::do_on_ssl_info(const SSL *ssl, int where, int ret) {
    (void)ssl;
    int w = where & -SSL_ST_MASK;
    const char *role;

    if ((w & SSL_ST_CONNECT) != 0)
        role = "client";
    else if ((w & SSL_ST_ACCEPT) != 0)
        role = "server";
    else
        role = "undefined";

    if ((where & SSL_CB_LOOP) != 0) {
        spdlog::debug("[role:{}, action:{}]", role, SSL_state_string_long(ssl_));
    } else if ((where & SSL_CB_ALERT) != 0) {
        const char *alertType;

        switch (*SSL_alert_type_string(ret)) {
            case 'W':
                alertType = "warning";
                break;

            case 'F':
                alertType = "fatal";
                break;

            default:
                alertType = "undefined";
        }

        if ((where & SSL_CB_READ) != 0) {
            spdlog::warn("received DTLS {} alert: {}", alertType, SSL_alert_desc_string_long(ret));
            if ((ret & 0xff) == SSL3_AD_CLOSE_NOTIFY) {
            }
        } else if ((where & SSL_CB_WRITE) != 0) {
            spdlog::warn("sending DTLS {} alert: {}", alertType, SSL_alert_desc_string_long(ret));
        } else {
            spdlog::debug("DTLS {} alert: {}", alertType, SSL_alert_desc_string_long(ret));
        }
    } else if ((where & SSL_CB_EXIT) != 0) {
        if (ret == 0)
            spdlog::debug("[role:{}, failed:'{}']", role, SSL_state_string_long(ssl_));
        else if (ret < 0)
            spdlog::debug("role: {}, waiting:'{}']", role, SSL_state_string_long(ssl_));
    } else if ((where & SSL_CB_HANDSHAKE_START) != 0) {
        spdlog::debug("DTLS handshake start");
    } else if ((where & SSL_CB_HANDSHAKE_DONE) != 0) {
        spdlog::debug("DTLS handshake done");
        uint8_t material[SRTP_MASTER_KEY_LEN * 2] = {0};
        static const std::string dtls_label = "EXTRACTOR-dtls_srtp";
        auto ret = SSL_export_keying_material(ssl_, material, sizeof(material), dtls_label.c_str(),
                                              dtls_label.size(), NULL, 0, 0);
        if (ret <= 0) {
            dtls_state_channel_.close();
            timeout_timer_.cancel();
            return;
        }
        handshake_done_ = true;
        int srtp_key_len = 16;
        int srtp_salt_len = 14;

        size_t offset = 0;
        std::string client_master_key((char *)(material), srtp_key_len);
        offset += srtp_key_len;
        std::string server_master_key((char *)(material + offset), srtp_key_len);
        offset += srtp_key_len;
        std::string client_master_salt((char *)(material + offset), srtp_salt_len);
        offset += srtp_salt_len;
        std::string server_master_salt((char *)(material + offset), srtp_salt_len);

        srtp_recv_key_ = client_master_key + client_master_salt;
        srtp_send_key_ = server_master_key + server_master_salt;
        handshake_done_cb_(SRTP_AES128_CM_HMAC_SHA1_80, srtp_recv_key_, srtp_send_key_);

        dtls_state_channel_.close();
        timeout_timer_.cancel();
    }
}

unsigned int DtlsBoringSSLSession::on_ssl_dtls_timer(SSL *ssl, unsigned int timer_us) {
    (void)ssl;
    if (timer_us == 0)
        return 100000;
    else if (timer_us >= 4000000)
        return 4000000;
    else
        return 2 * timer_us;
}

boost::asio::awaitable<int32_t> DtlsBoringSSLSession::do_handshake(mode m,
                                                                   std::shared_ptr<DtlsCert> dtls_cert) {
    (void)m;
    dtls_cert_ = dtls_cert;
    ssl_ctx_ = SSL_CTX_new(DTLS_method());
    if (!ssl_ctx_) {
        co_return -1;
    }

    state_ = CONNECTING;
    handshake_done_ = false;

    auto conf = Config::get_instance();

    if (!SSL_CTX_use_PrivateKey(ssl_ctx_, dtls_cert_->get_pkey())) {
        spdlog::error("load dtls key file:{} failed.",
                      Utils::get_bin_path() + conf->get_cert_root() + "/server.crt");
        co_return -4;
    }

    if (!SSL_CTX_use_certificate(ssl_ctx_, dtls_cert_->get_cert())) {
        spdlog::error("load dtls cert file:{} failed.",
                      (Utils::get_bin_path() + conf->get_cert_root() + "/server.key"));
        co_return -5;
    }

    SSL_CTX_set_verify(ssl_ctx_, SSL_VERIFY_NONE, NULL);
    SSL_CTX_set_options(ssl_ctx_, SSL_OP_CIPHER_SERVER_PREFERENCE | SSL_OP_NO_TICKET |
                                      SSL_OP_SINGLE_ECDH_USE | SSL_OP_NO_QUERY_MTU);
    SSL_CTX_set_session_cache_mode(ssl_ctx_, SSL_SESS_CACHE_OFF);
    SSL_CTX_set_read_ahead(ssl_ctx_, 1);
    SSL_CTX_set_verify_depth(ssl_ctx_, 4);
    SSL_CTX_set_verify(ssl_ctx_, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                       on_ssl_certificate_verify);
    SSL_CTX_set_info_callback(ssl_ctx_, on_ssl_info);
    auto ret =
        SSL_CTX_set_cipher_list(ssl_ctx_, "DEFAULT:!NULL:!aNULL:!SHA256:!SHA384:!aECDH:!AESGCM+AES256:!aPSK");
    if (!ret) {
        co_return -6;
    }

    // For OpenSSL >= 1.0.2.
    // SSL_CTX_set_ecdh_auto(ssl_ctx_, 1);
    ret = SSL_CTX_set_tlsext_use_srtp(ssl_ctx_, "SRTP_AES128_CM_SHA1_80");
    if (0 != ret) {
        co_return -7;
    }

    ssl_ = SSL_new(ssl_ctx_);
    // 设置bio
    bio_read_ = BIO_new(BIO_s_mem());
    bio_write_ = BIO_new(BIO_s_mem());
    if (!bio_read_ || !bio_write_) {
        spdlog::error("create bio failed");
        if (bio_read_) {
            BIO_free(bio_read_);
            bio_read_ = nullptr;
        }

        if (bio_write_) {
            BIO_free(bio_write_);
            bio_write_ = nullptr;
        }
        SSL_free(ssl_);
        ssl_ = nullptr;
        co_return -8;
    }

    SSL_set_bio(ssl_, bio_read_, bio_write_);
    // 设置mtu
    SSL_set_mtu(ssl_, 1350);
    // 设置私有数据指针
    SSL_set_mode(ssl_, SSL_MODE_AUTO_RETRY);
    SSL_set_ex_data(ssl_, 0, this);
    SSL_set_accept_state(ssl_);
    SSL_do_handshake(ssl_);
    // 启动定时器
    auto self(shared_from_this());
    handshake_coroutine_running_ = true;
    boost::asio::co_spawn(
        worker_->get_io_context(),
        [this]() -> boost::asio::awaitable<void> {
            boost::system::error_code ec;
            while (1) {
                struct timeval dtls_timeout;
                int r = DTLSv1_get_timeout(ssl_, &dtls_timeout);  // NOLINT
                if (r != 1) {
                    timeout_timer_.expires_after(std::chrono::milliseconds(5));
                    co_await timeout_timer_.async_wait(
                        boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                    if (ec) {
                        break;
                    }
                    continue;
                }

                uint64_t timeout_ms = 0;
                timeout_ms =
                    (dtls_timeout.tv_sec * static_cast<uint64_t>(1000)) + (dtls_timeout.tv_usec / 1000);
                if (timeout_ms >= 30000) {
                    co_await dtls_state_channel_.async_send(boost::system::error_code{}, FAILED,
                                                            boost::asio::use_awaitable);
                    break;
                }

                co_await timeout_timer_.async_wait(
                    boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                if (ec) {
                    co_await dtls_state_channel_.async_send(boost::system::error_code{}, FAILED,
                                                            boost::asio::use_awaitable);
                    break;
                }

                // 超时
                // DTLSv1_handle_timeout is called when a DTLS handshake timeout expires.
                // If no timeout had expired, it returns 0. Otherwise, it retransmits the
                // previous flight of handshake messages and returns 1. If too many timeouts
                // had expired without progress or an error occurs, it returns -1.
                auto ret = DTLSv1_handle_timeout(ssl_);
                if (ret == 1) {
                    co_await send_pending_outgoing_dtls_packet();
                } else if (ret == -1) {
                    co_await dtls_state_channel_.async_send(boost::system::error_code{}, FAILED,
                                                            boost::asio::use_awaitable);
                    break;
                }
            }
            co_return;
        },
        [this, self](std::exception_ptr exp) {
            (void)exp;
            handshake_coroutine_running_ = false;
            handshaking_coroutine_exited_.close();
        });

    boost::system::error_code ec;
    co_await dtls_state_channel_.async_receive(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    timeout_timer_.cancel();
    // 设置超时回调
    // This function sets an optional callback function for controlling the timeout interval on the DTLS
    // protocol. The callback function will be called by DTLS for every new DTLS packet that is sent.
    // DTLS_set_timer_cb(ssl_, on_ssl_dtls_timer);
    if (!handshake_done_) {
        co_return -11;
    }
    co_return 0;
}

boost::asio::awaitable<void> DtlsBoringSSLSession::close() {
    if (closed_.test_and_set(std::memory_order_acquire)) {
        co_return;
    }

    if (handshake_coroutine_running_) {
        boost::system::error_code ec;
        timeout_timer_.cancel();
        if (dtls_state_channel_.is_open()) {
            dtls_state_channel_.close();
        }
        co_await handshaking_coroutine_exited_.async_receive(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    }
    co_return;
}

boost::asio::awaitable<bool> DtlsBoringSSLSession::process_dtls_packet(
    uint8_t *data, size_t len, UdpSocket *sock, const boost::asio::ip::udp::endpoint &remote_ep) {
    (void)sock;
    (void)remote_ep;
    int written = BIO_write(bio_read_, data, len);
    if (written <= 0) {
        co_return false;
    }
    SSL_do_handshake(ssl_);

    auto read = SSL_read(ssl_, encrypted_buf_, 1024 * 1024);
    if (read <= 0) {
        int err = SSL_get_error(ssl_, read);
        switch (err) {
            case SSL_ERROR_NONE:
                spdlog::debug("SSL_ERROR_NONE");
                break;
            case SSL_ERROR_SSL:
                spdlog::debug("SSL_ERROR_SSL");
                break;
            case SSL_ERROR_WANT_READ:
                spdlog::debug("SSL_ERROR_WANT_READ");
                break;
            case SSL_ERROR_WANT_WRITE:
                spdlog::debug("SSL_ERROR_WANT_WRITE");
                break;
            case SSL_ERROR_WANT_X509_LOOKUP:
                spdlog::debug("SSL_ERROR_WANT_X509_LOOKUP");
                break;
            case SSL_ERROR_SYSCALL:
                spdlog::debug("SSL_ERROR_SYSCALL");
                break;
            case SSL_ERROR_ZERO_RETURN:
                spdlog::debug("SSL_ERROR_ZERO_RETURN");
                break;
            case SSL_ERROR_WANT_CONNECT:
                spdlog::debug("SSL_ERROR_WANT_CONNECT");
                break;
            case SSL_ERROR_WANT_ACCEPT:
                spdlog::debug("SSL_ERROR_WANT_ACCEPT");
                break;
            default:
                spdlog::debug("SSL_ERROR_UNKNOWN");
                break;
        }
    }
    co_await send_pending_outgoing_dtls_packet();
    co_return true;
}

boost::asio::awaitable<void> DtlsBoringSSLSession::send_pending_outgoing_dtls_packet() {
    if (BIO_eof(bio_write_)) {
        co_return;
    }

    int64_t read;
    char *data = nullptr;
    read = BIO_get_mem_data(bio_write_, &data);  // NOLINT
    if (read <= 0) {
        co_return;
    }
    co_await session_.send_udp_msg((uint8_t *)data, read);
    // 发送出去
    BIO_reset(bio_write_);
    co_return;
}
