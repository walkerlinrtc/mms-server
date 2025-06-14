#pragma once
#include "spdlog/spdlog.h"

#include <memory>
#include <string>
#include <boost/asio/experimental/channel.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/asio/redirect_error.hpp>

#include "base/utils/utils.h"
#include "base/network/tcp_socket.hpp"
#include "base/wait_group.h"

#include "core/stream_session.hpp"

#include "protocol/rtmp/rtmp_handshake.hpp"
#include "protocol/rtmp/rtmp_chunk_protocol.hpp"

namespace mms {
class PublishApp;
class ThreadWorker;
class RtmpMediaSource;
class RtmpMessage;
class OriginPullConfig;

class RtmpPlayClientSession : public StreamSession, public SocketInterfaceHandler {
public:
    RtmpPlayClientSession(std::shared_ptr<PublishApp> app, ThreadWorker *worker, 
                          const std::string &domain_name, const std::string & app_name, const std::string & stream_name);
    virtual ~RtmpPlayClientSession();
    void on_socket_open(std::shared_ptr<SocketInterface> sock) override;
    void on_socket_close(std::shared_ptr<SocketInterface> sock) override;

    void start();
    void stop();
    void set_url(const std::string & url);
    void set_pull_config(std::shared_ptr<OriginPullConfig> pull_config) {
        pull_config_ = pull_config;
    }
    
    std::shared_ptr<RtmpMediaSource> get_rtmp_media_source();
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
    void update_active_timestamp();
    boost::asio::awaitable<bool> on_recv_rtmp_message(std::shared_ptr<RtmpMessage> rtmp_msg);
    boost::asio::awaitable<bool> send_acknowledge_msg_if_required();
    // 异步方式发送rtmp消息
    boost::asio::awaitable<bool> send_rtmp_message(const std::vector<std::shared_ptr<RtmpMessage>> & rtmp_msgs);

    boost::asio::awaitable<bool> handle_amf0_command(std::shared_ptr<RtmpMessage> rtmp_msg);
    boost::asio::awaitable<bool> handle_amf0_status_command(std::shared_ptr<RtmpMessage> rtmp_msg);
    boost::asio::awaitable<bool> handle_amf0_result_command(std::shared_ptr<RtmpMessage> rtmp_msg);
    
    bool handle_video_msg(std::shared_ptr<RtmpMessage> msg);
    bool handle_audio_msg(std::shared_ptr<RtmpMessage> msg);

    bool handle_amf0_data(std::shared_ptr<RtmpMessage> rtmp_msg);
    bool handle_acknowledgement(std::shared_ptr<RtmpMessage> rtmp_msg);
    bool handle_user_control_msg(std::shared_ptr<RtmpMessage> rtmp_msg);
private:
    std::shared_ptr<OriginPullConfig> pull_config_;
    std::shared_ptr<SocketInterface> conn_;
    std::unique_ptr<RtmpHandshake> handshake_;
    std::unique_ptr<RtmpChunkProtocol> chunk_protocol_;
    uint32_t window_ack_size_ = 80000000;

    std::string url_;
    std::shared_ptr<RtmpMediaSource> rtmp_media_source_;

    using SYNC_CHANNEL = boost::asio::experimental::channel<void(boost::system::error_code, bool)>;
    boost::asio::experimental::channel<void(boost::system::error_code, std::vector<std::shared_ptr<RtmpMessage>>, SYNC_CHANNEL*)> rtmp_msgs_channel_;

    boost::asio::steady_timer check_closable_timer_;
    int64_t last_active_time_ = Utils::get_current_ms();
    int32_t transaction_id_ = 1;

    WaitGroup wg_;
};

};