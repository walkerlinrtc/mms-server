/*
 * @Author: jbl19860422
 * @Date: 2023-12-24 14:01:41
 * @LastEditTime: 2023-12-27 21:39:48
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\server\rtmp\rtmp_server_session.hpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include "spdlog/spdlog.h"

#include <memory>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/experimental/channel.hpp>

#include "base/utils/utils.h"
#include "base/wait_group.h"
#include "core/stream_session.hpp"

#include "protocol/rtmp/rtmp_handshake.hpp"
#include "protocol/rtmp/rtmp_chunk_protocol.hpp"

#include "rtmp/rtmp_message/command_message/rtmp_set_peer_bandwidth_message.hpp"
#include "rtmp/rtmp_message/command_message/rtmp_window_ack_size_message.hpp"
#include "rtmp/rtmp_message/chunk_message/rtmp_set_chunk_size_message.hpp"
#include "rtmp/rtmp_message/command_message/rtmp_connect_command_message.hpp"
#include "rtmp/rtmp_message/command_message/rtmp_publish_message.hpp"
#include "rtmp/rtmp_message/command_message/rtmp_play_message.hpp"
#include "rtmp/rtmp_message/command_message/rtmp_connect_resp_message.hpp"
#include "rtmp/rtmp_message/command_message/rtmp_release_stream_message.hpp"
#include "rtmp/rtmp_message/command_message/rtmp_release_stream_resp_message.hpp"
#include "rtmp/rtmp_message/command_message/rtmp_fcpublish_message.hpp"
#include "rtmp/rtmp_message/command_message/rtmp_fcpublish_resp_message.hpp"
#include "rtmp/rtmp_message/command_message/rtmp_create_stream_message.hpp"
#include "rtmp/rtmp_message/command_message/rtmp_create_stream_resp_message.hpp"
#include "rtmp/rtmp_message/command_message/rtmp_onstatus_message.hpp"
#include "rtmp/rtmp_message/command_message/user_ctrl_message/stream_begin_message.hpp"
#include "rtmp/rtmp_message/command_message/rtmp_access_sample_message.hpp"
#include "rtmp/rtmp_message/data_message/rtmp_metadata_message.hpp"

#include "base/obj_tracker.hpp"

namespace mms {
class SocketInterface;
class RtmpMediaSource;
class RtmpMediaSink;

class RtmpServerSession : public StreamSession, public ObjTracker<RtmpServerSession> {
public:
    RtmpServerSession(std::shared_ptr<SocketInterface> conn);
    virtual ~RtmpServerSession();
    void start() override;
    void stop() override;
    Json::Value to_json() override;
protected:
    // 同步方式发送rtmp消息，发送完成后会等待
    template<typename T>
    boost::asio::awaitable<bool> send_rtmp_message(const T & msg) {
        boost::system::error_code ec;
        std::shared_ptr<RtmpMessage> rtmp_msg = msg.encode();
        if (!rtmp_msg) {
            co_return false;
        }

        boost::asio::experimental::channel<void(boost::system::error_code, bool)> wait_channel(conn_->get_worker()->get_io_context());
        std::vector<std::shared_ptr<RtmpMessage>> v = {rtmp_msg};
        co_await rtmp_msgs_channel_.async_send(boost::system::error_code{}, v, &wait_channel, boost::asio::use_awaitable);
        co_await wait_channel.async_receive(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        co_return true;
    }
private:
    boost::asio::awaitable<bool> on_recv_rtmp_message(std::shared_ptr<RtmpMessage> rtmp_msg);
    boost::asio::awaitable<bool> send_acknowledge_msg_if_required();
    boost::asio::awaitable<bool> handle_protocol_control_message(std::shared_ptr<RtmpMessage> msg);
    // 异步方式发送rtmp消息
    boost::asio::awaitable<bool> send_rtmp_message(const std::vector<std::shared_ptr<RtmpMessage>> & rtmp_msgs);
    boost::asio::awaitable<bool> handle_amf0_command(std::shared_ptr<RtmpMessage> rtmp_msg);
    boost::asio::awaitable<bool> handle_amf0_connect_command(std::shared_ptr<RtmpMessage> rtmp_msg);
    boost::asio::awaitable<bool> handle_amf0_release_stream_command(std::shared_ptr<RtmpMessage> rtmp_msg);
    boost::asio::awaitable<bool> handle_amf0_fcpublish_command(std::shared_ptr<RtmpMessage> rtmp_msg);
    boost::asio::awaitable<bool> handle_amf0_createstream_command(std::shared_ptr<RtmpMessage> rtmp_msg);
    boost::asio::awaitable<bool> handle_amf0_publish_command(std::shared_ptr<RtmpMessage> rtmp_msg);
    boost::asio::awaitable<bool> handle_amf0_play_command(std::shared_ptr<RtmpMessage> rtmp_msg);
    bool handle_video_msg(std::shared_ptr<RtmpMessage> msg);
    bool handle_audio_msg(std::shared_ptr<RtmpMessage> msg);
    bool handle_amf0_data(std::shared_ptr<RtmpMessage> rtmp_msg);
    bool handle_acknowledgement(std::shared_ptr<RtmpMessage> rtmp_msg);
    bool handle_user_control_msg(std::shared_ptr<RtmpMessage> rtmp_msg);
    bool parse_connect_cmd(RtmpConnectCommandMessage & connect_command);
    bool parse_publish_cmd(RtmpPublishMessage & pub_cmd);
    bool parse_play_cmd(RtmpPlayMessage & play_cmd);
private:
    void start_alive_checker();
    void start_statistic_timer();
    void update_active_timestamp();
    void start_recv_coroutine();
    void start_send_coroutine();
private:
    void detach_source();// 将数据迁移到其他session，可能是重推
private:
    std::shared_ptr<SocketInterface> conn_;
    RtmpHandshake handshake_;
    RtmpChunkProtocol chunk_protocol_;
    int64_t window_ack_size_ = 5*1024*1024;

    std::shared_ptr<RtmpMediaSink> rtmp_media_sink_;
    std::shared_ptr<RtmpMediaSource> rtmp_media_source_;
    bool is_publisher_ = false;
    bool is_player_ = false;

    using SYNC_CHANNEL = boost::asio::experimental::channel<void(boost::system::error_code, bool)>;
    boost::asio::experimental::channel<void(boost::system::error_code, 
                                            std::vector<std::shared_ptr<RtmpMessage>>, 
                                            SYNC_CHANNEL*)> rtmp_msgs_channel_;

    int64_t last_active_time_ = Utils::get_current_ms();
    boost::asio::steady_timer alive_timeout_timer_;//超时定时器，如果超过一段时间，没有任何数据，则关闭session 
    boost::asio::steady_timer statistic_timer_;//统计定时器
    WaitGroup wg_;
};

};