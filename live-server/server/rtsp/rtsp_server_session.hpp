/*
 * @Author: jbl19860422
 * @Date: 2023-12-26 22:24:34
 * @LastEditTime: 2023-12-30 11:38:38
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\server\rtsp\rtsp_server_session.hpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include <memory>
#include <atomic>
#include <array>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/asio/steady_timer.hpp>

#include "protocol/rtsp/rtsp_define.hpp"
#include "core/stream_session.hpp"
#include "base/wait_group.h"
#include "base/obj_tracker.hpp"

namespace mms {
class SocketInterface;
class RtspRequest;
class RtspResponse;
class ThreadWorker;
class RtspMediaSource;
class RtspMediaSink;
class RtpPacket;

class RtspServerSession : public StreamSession, public ObjTracker<RtspServerSession> {
public:
    RtspServerSession(std::shared_ptr<SocketInterface> sock);
    virtual ~RtspServerSession();
    // session interface
    void start() override;
    void stop() override;

    boost::asio::awaitable<bool> on_rtsp_request(std::shared_ptr<RtspRequest> req);
    boost::asio::awaitable<bool> on_rtsp_response(std::shared_ptr<RtspResponse> req);
    boost::asio::awaitable<bool> process_options_req(std::shared_ptr<RtspRequest> req);
    boost::asio::awaitable<bool> process_announce_req(std::shared_ptr<RtspRequest> req);
    boost::asio::awaitable<bool> process_describe_req(std::shared_ptr<RtspRequest> req);
    boost::asio::awaitable<bool> process_setup_req(std::shared_ptr<RtspRequest> req);
    boost::asio::awaitable<bool> process_record_req(std::shared_ptr<RtspRequest> req);
    boost::asio::awaitable<bool> process_play_req(std::shared_ptr<RtspRequest> req);
    boost::asio::awaitable<bool> process_teardown_req(std::shared_ptr<RtspRequest> req);
private:
    void start_recv_coroutine();
    boost::asio::awaitable<bool> send_rtsp_resp(std::shared_ptr<RtspResponse> resp);
    boost::asio::awaitable<bool> send_rtp_over_tcp_pkts(std::vector<std::shared_ptr<RtpPacket>> pkts);
private:
    size_t recv_buf_size_ = 0;
    std::unique_ptr<char[]> recv_buf_;
    std::unique_ptr<char[]> send_buf_;

    std::atomic_flag closed_ = ATOMIC_FLAG_INIT;
    std::shared_ptr<SocketInterface> sock_;
    std::shared_ptr<RtspRequest> rtsp_request_;
    std::shared_ptr<RtspResponse> rtsp_response_;
    std::shared_ptr<RtspMediaSource> rtsp_media_source_;
    std::shared_ptr<RtspMediaSink> rtsp_media_sink_;
    std::shared_ptr<RtspMediaSource> play_source_;

    E_RTSP_STATE state_ = E_RTSP_STATE_INIT;
    bool is_tcp_transport_ = false;
    bool is_udp_transport_ = false;
    bool is_unicast_ = false;
    bool is_multicast_ = false;
    uint16_t video_port_[2];
    uint16_t audio_port_[2];
    uint64_t session_id_;
    bool is_publisher_ = false;
    bool is_player_ = false;
    
    boost::asio::experimental::channel<void(boost::system::error_code, std::function<boost::asio::awaitable<bool>()>)> send_funcs_channel_;
    boost::asio::steady_timer play_sdp_timeout_timer_;
    WaitGroup wg_;
};

};