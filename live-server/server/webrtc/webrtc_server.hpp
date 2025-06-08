#pragma once
#include <iostream>
#include <map>
#include <memory>
#include <unordered_map>

#include "server/udp/udp_server.hpp"
#include "srtp/srtp_session.h"


namespace mms {
class WebSocketConn;
class WebRtcServerSession;
class WsConn;
class StunMsg;
class ThreadWorker;
class DtlsCert;
class HttpRequest;
class HttpResponse;

struct hash_endpoint {
    size_t operator()(const boost::asio::ip::udp::endpoint &p) const {
        return std::hash<std::string>()(p.address().to_string()) ^ std::hash<int>()(p.port());
    }
};

class WebRtcServerSessionCloseHandler {
public:
    virtual void on_webrtc_session_close(std::shared_ptr<WebRtcServerSession>) = 0;
};

class WebRtcServer : public UdpServer, public WebRtcServerSessionCloseHandler {
public:
    WebRtcServer(const std::vector<ThreadWorker *> &workers) : UdpServer(workers), workers_(workers) {}

    virtual ~WebRtcServer() { srtp_shutdown(); }

    bool start(const std::string &listen_ip, const std::string &extern_ip, uint16_t port);
    void stop();

    boost::asio::awaitable<void> on_whip(std::shared_ptr<HttpRequest> req,
                                         std::shared_ptr<HttpResponse> resp);
    boost::asio::awaitable<void> on_whep(std::shared_ptr<HttpRequest> req,
                                         std::shared_ptr<HttpResponse> resp);
    static std::shared_ptr<DtlsCert> get_default_dtls_cert();

private:
    boost::asio::awaitable<void> on_udp_socket_recv(UdpSocket *sock, std::unique_ptr<uint8_t[]> data,
                                                    size_t len,
                                                    boost::asio::ip::udp::endpoint &remote_ep) override;
    boost::asio::awaitable<bool> process_stun_packet(std::shared_ptr<StunMsg> stun_msg,
                                                     std::unique_ptr<uint8_t[]> data, size_t len,
                                                     UdpSocket *sock,
                                                     const boost::asio::ip::udp::endpoint &remote_ep);

private:
    void on_webrtc_session_close(std::shared_ptr<WebRtcServerSession> session);

private:
    uint64_t get_endpoint_hash(const boost::asio::ip::udp::endpoint &ep);

private:
    bool init_srtp();
    bool init_certs();

private:
    std::string listen_ip_;
    std::string extern_ip_;
    uint16_t listen_udp_port_;
    int32_t using_udp_socket_index_ = 0;
    std::atomic<uint32_t> using_worker_ = 0;
    std::vector<ThreadWorker *> workers_;
    std::mutex mtx_;
    std::mutex session_map_mtx_;
    std::unordered_map<uint64_t, std::shared_ptr<WebRtcServerSession>> endpoint_session_map_;
    std::multimap<WebRtcServerSession *, uint64_t> session_endpoint_map_;
    std::unordered_map<std::string, std::shared_ptr<WebRtcServerSession>> ufrag_session_map_;

    static std::shared_ptr<DtlsCert> default_dtls_cert_;
};
};  // namespace mms