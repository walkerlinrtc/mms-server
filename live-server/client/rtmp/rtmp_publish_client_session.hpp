#pragma once

#include <memory>
#include <boost/asio/experimental/channel.hpp>
#include <boost/algorithm/string.hpp>
#include "base/wait_group.h"
#include "base/utils/utils.h"

namespace mms {
class EdgePushConfig;
class RtmpMediaSource;
class RtmpMediaSink;
class PublishApp;

class RtmpPublishClientSession : public StreamSession, public SocketInterfaceHandler {
public:
    RtmpPublishClientSession(std::weak_ptr<RtmpMediaSource> source, std::shared_ptr<PublishApp> app, ThreadWorker *worker);
    virtual ~RtmpPublishClientSession();
    void on_socket_open(std::shared_ptr<SocketInterface> sock) override;
    void on_socket_close(std::shared_ptr<SocketInterface> sock) override;
    void service() override;
    void close() override;
    void set_url(const std::string & url);
    void set_push_config(std::shared_ptr<EdgePushConfig> push_config) {
        push_config_ = push_config;
    }
    std::shared_ptr<RtmpMediaSink> get_rtmp_media_sink();
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
    void start_alive_checker();
    
    boost::asio::awaitable<bool> send_acknowledge_msg_if_required();
    boost::asio::awaitable<bool> on_recv_rtmp_message(std::shared_ptr<RtmpMessage> rtmp_msg);
    // 异步方式发送rtmp消息
    boost::asio::awaitable<bool> send_rtmp_message(const std::vector<std::shared_ptr<RtmpMessage>> & rtmp_msgs);
    boost::asio::awaitable<bool> handle_amf0_command(std::shared_ptr<RtmpMessage> rtmp_msg);
    boost::asio::awaitable<bool> handle_amf0_status_command(std::shared_ptr<RtmpMessage> rtmp_msg);
    boost::asio::awaitable<bool> handle_amf0_result_command(std::shared_ptr<RtmpMessage> rtmp_msg);
    bool handle_acknowledgement(std::shared_ptr<RtmpMessage> rtmp_msg);
    bool handle_user_control_msg(std::shared_ptr<RtmpMessage> rtmp_msg);

    std::shared_ptr<SocketInterface> conn_;
    std::unique_ptr<RtmpHandshake> handshake_;
    std::unique_ptr<RtmpChunkProtocol> chunk_protocol_;
    uint32_t window_ack_size_ = 80000000;
private:
    std::shared_ptr<EdgePushConfig> push_config_;
    std::string url_;

    std::weak_ptr<RtmpMediaSource> rtmp_source_;
    std::shared_ptr<RtmpMediaSink> rtmp_media_sink_;

    using SYNC_CHANNEL = boost::asio::experimental::channel<void(boost::system::error_code, bool)>;
    boost::asio::experimental::channel<void(boost::system::error_code, std::vector<std::shared_ptr<RtmpMessage>>, SYNC_CHANNEL*)> rtmp_msgs_channel_;

    int64_t last_active_time_ = Utils::get_current_ms();
    boost::asio::steady_timer check_closable_timer_;
    boost::asio::steady_timer alive_timeout_timer_;
    int32_t transaction_id_ = 1;
    WaitGroup wg_;
};

};