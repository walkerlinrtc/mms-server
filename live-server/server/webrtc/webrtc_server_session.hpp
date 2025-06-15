#pragma once
#include <memory>
#include <map>
#include <future>

#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/awaitable.hpp>

#include "json/json.h"

#include "udp_msg_demultiplex.hpp"
#include "core/stream_session.hpp"
#include "protocol/sdp/sdp.hpp"
#include "base/network/udp_socket.hpp"
#include "srtp/srtp_session.h"

#include "protocol/rtp/rtp_h264_depacketizer.h"
#include "protocol/rtp/rtcp/rtcp_sr.h"
#include "base/utils/utils.h"
#include "base/wait_group.h"

#include "base/obj_tracker.hpp"

namespace mms {
class WebRtcServerSession;
class WebRtcServerSessionCloseHandler;
class WebRtcMediaSource;
class RtcpPacket;
class RtcpFbPacket;
class WsConn;
class HttpConn;
class SocketInterface;
class WebSocketPacket;
class DtlsBoringSSLSession;
class ThreadWorker;
class UdpSocket;
class StunMsg;
class RtpMediaSink;
class DtlsCert;
class HttpRequest;
class HttpResponse;

class WebRtcServerSession  : public StreamSession, public ObjTracker<WebRtcServerSession> {
public:
    WebRtcServerSession(ThreadWorker *worker);
    virtual ~WebRtcServerSession();
    void set_close_handler(WebRtcServerSessionCloseHandler *close_handler) {
        close_handler_ = close_handler;
    }

    void start() override;
    void stop() override;
    
    ThreadWorker *get_worker() {
        return worker_;
    }

    void set_etag(const std::string & etag) {
        etag_ = etag;
    }

    const std::string & get_local_ice_ufrag() const {
        return local_ice_ufrag_;
    }

    const std::string & get_local_ice_pwd() const {
        return local_ice_pwd_;
    }

    void set_remote_ice_ufrag(const std::string & v) {
        remote_ice_ufrag_ = v;
    }

    void set_remote_ice_pwd(const std::string & v) {
        remote_ice_pwd_ = v;
    }

    const std::string & get_remote_ice_ufrag() const {
        return remote_ice_ufrag_;
    }

    const std::string & get_remote_ice_pwd() const {
        return remote_ice_pwd_;
    }

    std::string get_local_ip() const;
    void set_local_ip(const std::string & v);

    uint16_t get_udp_port() const;
    void set_udp_port(uint16_t v);

    std::shared_ptr<DtlsCert> get_dtls_cert() {
        return dtls_cert_;
    }

    void set_dtls_cert(std::shared_ptr<DtlsCert> dtls_cert);

    void set_send_socket(UdpSocket *socket) {
        send_rtp_socket_ = socket;
    }
    
    boost::asio::awaitable<bool> process_stun_packet(std::shared_ptr<StunMsg> stun_msg, std::unique_ptr<uint8_t[]> data, size_t len, UdpSocket *sock, const boost::asio::ip::udp::endpoint &remote_ep);
    boost::asio::awaitable<bool> process_dtls_packet(std::unique_ptr<uint8_t[]> data, size_t len, UdpSocket *sock, const boost::asio::ip::udp::endpoint &remote_ep);
    boost::asio::awaitable<bool> process_srtp_packet(std::unique_ptr<uint8_t[]> data, size_t len, UdpSocket *sock, const boost::asio::ip::udp::endpoint &remote_ep);
    boost::asio::awaitable<void> async_process_udp_msg(UdpSocket *sock, std::unique_ptr<uint8_t[]> data, size_t len, boost::asio::ip::udp::endpoint &remote_ep);
    boost::asio::awaitable<bool> send_udp_msg(const uint8_t *data, size_t len);
public:
    boost::asio::awaitable<bool> process_whip_req(std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp);
    boost::asio::awaitable<bool> process_whep_req(std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp);
    boost::asio::awaitable<bool> process_whep_patch_req(std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp);
    void update_active_timestamp();
private:
    void start_alive_checker();
    boost::asio::awaitable<void> stop_alive_checker();
    void start_process_recv_udp_msg();
    boost::asio::awaitable<void> stop_process_recv_udp_msg();
    void start_pli_sender();
    boost::asio::awaitable<void> stop_pli_sender();
    void start_rtp_sender();
    boost::asio::awaitable<void> stop_rtp_sender();
    void start_rtcp_fb_sender();
    boost::asio::awaitable<void> stop_rtcp_fb_sender();
    void start_rtcp_sender();
    boost::asio::awaitable<void> stop_rtcp_sender();

    boost::asio::awaitable<int32_t> process_stun_binding_req(std::shared_ptr<StunMsg> stun_msg, UdpSocket *sock, const boost::asio::ip::udp::endpoint &remote_ep);
    void on_dtls_handshake_done(SRTPProtectionProfile profile, const std::string & srtp_recv_key, const std::string & srtp_send_key);
    bool find_key_frame(uint32_t timestamp, std::shared_ptr<RtpH264NALU> & nalu);
private:
    ThreadWorker *worker_;
    std::string etag_;
    std::string local_ip_;
    uint16_t udp_port_;

    std::shared_ptr<WebRtcMediaSource> webrtc_media_source_;
    std::shared_ptr<RtpMediaSink> rtp_media_sink_;
    bool ws_msgs_coroutine_running_ = false;
    boost::asio::experimental::concurrent_channel<void(boost::system::error_code, bool)> ws_msgs_coroutine_exited_;

    Sdp remote_sdp_;
    Sdp local_sdp_;
    uint64_t session_id_ = 0;
    std::string stream_;
    std::string local_ice_ufrag_;
    std::string local_ice_pwd_;
    std::string remote_ice_ufrag_;
    std::string remote_ice_pwd_;

    std::shared_ptr<DtlsCert> dtls_cert_;
    SRTPSession srtp_session_;

    uint8_t audio_pt_ = 111;
    uint8_t video_pt_ = 127;

    uint32_t video_ssrc_ = 0;
    uint32_t audio_ssrc_ = 0;

    WebRtcServerSessionCloseHandler *close_handler_ = nullptr;
    bool is_publisher_{false};
    bool is_player_{false};
    uint32_t rtcp_pkt_count_ = 0;

    boost::asio::experimental::concurrent_channel<void(boost::system::error_code, UdpSocket *, std::unique_ptr<uint8_t[]>, size_t, boost::asio::ip::udp::endpoint)> udp_msgs_channel_;

    int64_t last_active_time_ = Utils::get_current_ms();
    boost::asio::steady_timer alive_timeout_timer_;//超时定时器，如果超过一段时间，没有任何数据，则关闭session 

    boost::asio::steady_timer play_sdp_timeout_timer_;
    
    UdpSocket *send_rtp_socket_;
    boost::asio::ip::udp::endpoint peer_ep;
    
    
    std::unique_ptr<uint8_t[]> send_buf_;
    std::unique_ptr<uint8_t[]> enc_buf_;

    boost::asio::experimental::concurrent_channel<void(boost::system::error_code, std::vector<std::shared_ptr<RtpPacket>>)> send_rtp_pkts_channel_;
    
    boost::asio::experimental::concurrent_channel<void(boost::system::error_code, std::shared_ptr<RtcpFbPacket>)> send_rtcp_fb_pkts_channel_;

    boost::asio::experimental::concurrent_channel<void(boost::system::error_code, std::shared_ptr<RtcpPacket>)> send_rtcp_pkts_channel_;

    boost::asio::experimental::channel<void(boost::system::error_code, std::function<boost::asio::awaitable<bool>()>)> send_funcs_channel_;

    std::unique_ptr<uint8_t[]> fb_buf_;
    std::unique_ptr<uint8_t[]> enc_fb_buf_;

    boost::asio::steady_timer send_pli_timer_;

    RtpH264Depacketizer rtp_h264_depacketizer_;
    uint32_t first_rtp_video_ts_ = 0;
    uint32_t first_rtp_audio_ts_ = 0;

    std::shared_ptr<DtlsBoringSSLSession> dtls_boringssl_session_;

    std::unordered_map<uint32_t, RtcpSr> recv_sr_packets_;
    int32_t video_extended_highest_sequence_number_received_ = 0;
    int32_t audio_extended_highest_sequence_number_received_ = 0;

    int32_t video_recv_rtp_ts_ = 0;
    int64_t video_recv_in_timestamp_units_ = 0;
    int64_t video_send_ts_ = 0;
    int32_t video_last_sr_timestamp_ = 0;

    int32_t audio_recv_rtp_ts_ = 0;
    int64_t audio_recv_in_timestamp_units_ = 0;
    int64_t audio_send_ts_ = 0;
    int32_t audio_last_sr_timestamp_ = 0;

    int32_t video_interarrival_jitter_ = 0;
    int32_t audio_interarrival_jitter_ = 0;

    uint64_t video_last_sr_ntp_ = 0;
    uint64_t audio_last_sr_ntp_ = 0;

    uint64_t video_last_sr_sys_ntp_ = 0;
    uint64_t audio_last_sr_sys_ntp_ = 0;
    WaitGroup wg_;
};

};