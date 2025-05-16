/*
 * @Author: jbl19860422
 * @Date: 2023-08-26 10:00:29
 * @LastEditTime: 2023-08-27 22:00:37
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\server\webrtc\dtls_boringssl\dtls_boringssl_session.h
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include <functional>
#include <atomic>

#include <boost/asio/ip/udp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/asio/steady_timer.hpp>

#include "openssl/ssl.h"
#include "openssl/bio.h"
#include "openssl/dtls1.h"

#include "define.h"
#include "base/network/udp_socket.hpp"

namespace mms {
class ThreadWorker;
class WebRtcServerSession;
class DtlsCert;
class DtlsBoringSSLSession : public std::enable_shared_from_this<DtlsBoringSSLSession> {
public:
    enum mode {
        mode_client = 0,
        mode_server = 1,
    };

    enum DtlsState
    {
        NEW = 1,
        CONNECTING,
        CONNECTED,
        FAILED,
        CLOSED
    };
    
public:
    DtlsBoringSSLSession(ThreadWorker *worker, WebRtcServerSession & session);
    virtual ~DtlsBoringSSLSession();
public:
    boost::asio::awaitable<int32_t> do_handshake(mode m, std::shared_ptr<DtlsCert> dtls_cert);
    boost::asio::awaitable<void> close();
    boost::asio::awaitable<bool> process_dtls_packet(uint8_t *data, size_t len, UdpSocket *sock, const boost::asio::ip::udp::endpoint &remote_ep);
    void on_handshake_done(const std::function<void(SRTPProtectionProfile profile, const std::string & srtp_recv_key, const std::string & srtp_send_key)> & cb) {
        handshake_done_cb_ = cb;
    }
private:
    boost::asio::awaitable<void> send_pending_outgoing_dtls_packet();
    std::function<void(SRTPProtectionProfile profile, const std::string & srtp_recv_key, const std::string & srtp_send_key)> handshake_done_cb_;
private:
    ThreadWorker * worker_ = nullptr;
    mode mode_ = mode_server;
    SSL *ssl_ = nullptr;
    BIO *bio_read_    = nullptr;
    BIO *bio_write_   = nullptr;
    SSL_CTX *ssl_ctx_ = nullptr;
    uint8_t *encrypted_buf_ = nullptr;

    DtlsState state_ = NEW;
    bool handshake_done_ = false;
    boost::asio::steady_timer timeout_timer_;
    WebRtcServerSession & session_;
    std::shared_ptr<DtlsCert> dtls_cert_;
private:
    static int on_ssl_certificate_verify(int preverifyOk, X509_STORE_CTX* ctx);
    static void on_ssl_info(const SSL* ssl, int where, int ret);
    void do_on_ssl_info(const SSL* ssl, int where, int ret);
    static unsigned int on_ssl_dtls_timer(SSL* ssl, unsigned int timer_us);

    std::atomic_flag closed_ = ATOMIC_FLAG_INIT;
    boost::asio::experimental::concurrent_channel<void(boost::system::error_code, DtlsState)> dtls_state_channel_;
    bool handshake_coroutine_running_ = false;
    boost::asio::experimental::concurrent_channel<void(boost::system::error_code, bool)> handshaking_coroutine_exited_;

    std::string srtp_recv_key_;
    std::string srtp_send_key_;
};
};