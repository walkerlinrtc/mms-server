#include "webrtc_server.hpp"
#include "webrtc_server_session.hpp"
#include "config/config.h"

#include "dtls/dtls_cert.h"
#include "server/stun/protocol/stun_msg.h"
#include "server/stun/protocol/stun_binding_response_msg.hpp"
#include "server/stun/protocol/stun_mapped_address_attr.h"
#include "udp_msg_demultiplex.hpp"
#include <srtp2/srtp.h>

#include "protocol/http/http_request.hpp"
#include "protocol/http/http_response.hpp"
#include "log/log.h"

#include "base/thread/thread_pool.hpp"
using namespace mms;

std::shared_ptr<DtlsCert> WebRtcServer::default_dtls_cert_;
uint64_t WebRtcServer::get_endpoint_hash(const boost::asio::ip::udp::endpoint& ep) 
{
    uint64_t v = 0;
    v = ep.address().to_v4().to_uint();
    v = v << 32;
    v |= ep.port();
    return v;
}

bool WebRtcServer::start(const std::string & listen_ip, const std::string & extern_ip, uint16_t port) {
    OpenSSL_add_all_algorithms();
    SSL_library_init();
    SSL_load_error_strings();
    
    bool ret = init_certs();
    if (!ret) {
        return false;
    }

    ret = init_srtp();
    if (!ret) {
        return false;
    }

    ret = UdpServer::start_listen(listen_ip, port);
    if (!ret) {
        return false;
    }
    listen_ip_ = listen_ip;
    extern_ip_ = extern_ip;
    listen_udp_port_ = port;
    return true;
}

bool WebRtcServer::init_srtp()
{
    auto err = srtp_init();
    if (err == srtp_err_status_ok)
    {
        //todo call srtp_install_event_handler
    }

    if (err != srtp_err_status_ok)
    {
        return false;
    }
    return true;
}

bool WebRtcServer::init_certs()
{
    std::string domain = "test.publish.com";
    default_dtls_cert_ = std::make_shared<DtlsCert>();
    if (!default_dtls_cert_->init(domain))
    {
        CORE_ERROR("webrtc server init dtls cert failed");
        return false;
    }
    CORE_INFO("webrtc server init dtls cert succeed!!!");
    return true;
}

std::shared_ptr<DtlsCert> WebRtcServer::get_default_dtls_cert() {
    return default_dtls_cert_;
}

boost::asio::awaitable<void> WebRtcServer::on_udp_socket_recv(UdpSocket *sock, std::unique_ptr<uint8_t[]> data, size_t len, boost::asio::ip::udp::endpoint &remote_ep)
{
    UDP_MSG_TYPE msg_type = detect_msg_type(data.get(), len);
    if (UDP_MSG_STUN == msg_type) {
        std::shared_ptr<StunMsg> stun_msg = std::make_shared<StunMsg>();
        int32_t ret = stun_msg->decode(data.get(), len);
        if (0 == ret) {// stun消息虽然最后交给session，但是可以在本协程内部处理，不需要在session内部处理
            if (co_await process_stun_packet(stun_msg, std::move(data), len, sock, remote_ep)) {
                co_return;
            }
        } else {
            spdlog::error("decode stun msg failed, ret:{}", ret);
        }
    } else {
        // 找到session
        std::shared_ptr<WebRtcServerSession> session;
        {
            std::lock_guard<std::mutex> lck(session_map_mtx_);
            uint64_t key = get_endpoint_hash(remote_ep);
            auto it_session = endpoint_session_map_.find(key);
            if (it_session == endpoint_session_map_.end())
            {
                co_return;
            }
            session = it_session->second;
        }

        if (!session) // todo add log
        {
            co_return;
        }
        // 交给session异步处理
        co_await session->async_process_udp_msg(sock, std::move(data), len, remote_ep);
    }
    co_return;
}

boost::asio::awaitable<bool> WebRtcServer::process_stun_packet(std::shared_ptr<StunMsg> stun_msg, std::unique_ptr<uint8_t[]> data, size_t len, UdpSocket *sock, const boost::asio::ip::udp::endpoint &remote_ep)
{
    // 校验完整性
    auto username_attr = stun_msg->get_username_attr();
    if (!username_attr) {
        spdlog::error("process_stun_packet failed, no username");
        co_return false;
    }

    const std::string & local_user_name = username_attr.value().get_local_user_name();
    if (local_user_name.empty()) {
        spdlog::error("process_stun_packet failed, no local_user_name");
        co_return false;
    }

    std::shared_ptr<WebRtcServerSession> webrtc_session;
    {
        std::lock_guard<std::mutex> lck(session_map_mtx_);
        auto it_session = ufrag_session_map_.find(local_user_name);
        if (it_session == ufrag_session_map_.end()) {
            co_return false;
        }

        webrtc_session = it_session->second;
        uint64_t key = get_endpoint_hash(remote_ep);

        endpoint_session_map_.insert(std::pair(key, webrtc_session));
        session_endpoint_map_.insert(std::pair(webrtc_session.get(), key));
    }

    if (!webrtc_session) // todo add log
    {
        CORE_ERROR("could not find session for ufrag:{}", local_user_name);
        co_return false;
    }
    
    auto ret = co_await webrtc_session->process_stun_packet(stun_msg, std::move(data), len, sock, remote_ep);
    co_return ret;
}

boost::asio::awaitable<void> WebRtcServer::on_whip(std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp) {
    auto webrtc_session = std::make_shared<WebRtcServerSession>(thread_pool_inst::get_mutable_instance().get_worker(-1));
    webrtc_session->set_local_ip(extern_ip_);
    webrtc_session->set_udp_port(listen_udp_port_);

    using_udp_socket_index_ = (using_udp_socket_index_+1)%udp_socks_.size();
    webrtc_session->set_send_socket(udp_socks_[using_udp_socket_index_]);
    {
        std::lock_guard<std::mutex> lck(session_map_mtx_);
        ufrag_session_map_.insert(std::pair(webrtc_session->get_local_ice_ufrag(), webrtc_session));
    }

    webrtc_session->set_close_handler(this);
    webrtc_session->set_dtls_cert(default_dtls_cert_); // todo, find cert by domain
    if (!co_await webrtc_session->process_whip_req(req, resp)) {
        co_return;
    }
    webrtc_session->start();
    co_return;
}

boost::asio::awaitable<void> WebRtcServer::on_whep(std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp) {
    auto webrtc_session = std::make_shared<WebRtcServerSession>(thread_pool_inst::get_mutable_instance().get_worker(-1));
    webrtc_session->set_local_ip(extern_ip_);
    webrtc_session->set_udp_port(listen_udp_port_);

    using_udp_socket_index_ = (using_udp_socket_index_+1)%udp_socks_.size();
    webrtc_session->set_send_socket(udp_socks_[using_udp_socket_index_]);
    {
        std::lock_guard<std::mutex> lck(session_map_mtx_);
        auto etag = Utils::get_rand_str(16);
        ufrag_session_map_.insert(std::pair(webrtc_session->get_local_ice_ufrag(), webrtc_session));
        // etag_session_map_.insert(std::pair(etag, webrtc_session));
    }

    webrtc_session->set_close_handler(this);
    webrtc_session->set_dtls_cert(default_dtls_cert_); // todo, find cert by domain
    CORE_DEBUG("start process whep");
    if (!co_await webrtc_session->process_whep_req(req, resp)) {
        co_return;
    }
    webrtc_session->start();
    co_return;
}

std::string trim_quotes(const std::string& etag_raw) {
    if (etag_raw.length() >= 2 && etag_raw.front() == '"' && etag_raw.back() == '"') {
        return etag_raw.substr(1, etag_raw.length() - 2);
    }
    return etag_raw;
}

boost::asio::awaitable<void> WebRtcServer::on_whep_patch(std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp) {
    Sdp remote_sdp;
    auto etag = trim_quotes(req->get_header("If-Match"));
    std::shared_ptr<WebRtcServerSession> webrtc_session;
    {
        std::lock_guard<std::mutex> lck(session_map_mtx_);
        auto it = ufrag_session_map_.find(etag);
        if (it == ufrag_session_map_.end()) {
            spdlog::error("could not find webrtc session for:{}", etag);
            co_return;
        }
        webrtc_session = it->second;
    }
    
    if (!webrtc_session) {
        co_return;
    } 

    if (!co_await webrtc_session->process_whep_patch_req(req, resp)) {
        co_return;
    }
    co_return;
}

void WebRtcServer::on_webrtc_session_close(std::shared_ptr<WebRtcServerSession> session)
{
    WebRtcServerSession *s = session.get();
    {
        std::lock_guard<std::mutex> lck(session_map_mtx_);
        ufrag_session_map_.erase(session->get_local_ice_ufrag());

        auto it_begin = session_endpoint_map_.lower_bound(s);
        auto it_end = session_endpoint_map_.upper_bound(s);
        if (it_begin != std::end(session_endpoint_map_)) {
            for (auto iter = it_begin; iter != it_end; iter++) {
                uint64_t k = iter->second;
                endpoint_session_map_.erase(k);
            }
        }

        session_endpoint_map_.erase(s);
    }
    
}

void WebRtcServer::stop()
{
}