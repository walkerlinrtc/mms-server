#include "rtmp_play_client_session.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>

#include "app/publish_app.h"
#include "base/network/tcp_socket.hpp"
#include "base/network/tls_socket.hpp"
#include "config/app_config.h"
#include "config/cdn/origin_pull_config.h"
#include "core/rtmp_media_sink.hpp"
#include "core/rtmp_media_source.hpp"
#include "core/source_manager.hpp"
#include "core/stream_session.hpp"
#include "protocol/rtmp/amf0/amf0_inc.hpp"
#include "protocol/rtmp/rtmp_define.hpp"
#include "protocol/rtmp/rtmp_message/chunk_message/rtmp_acknowledge_message.hpp"
#include "protocol/rtmp/rtmp_message/chunk_message/rtmp_set_chunk_size_message.hpp"
#include "protocol/rtmp/rtmp_message/chunk_message/rtmp_window_acknowledge_size_message.hpp"
#include "protocol/rtmp/rtmp_message/command_message/rtmp_access_sample_message.hpp"
#include "protocol/rtmp/rtmp_message/command_message/rtmp_connect_command_message.hpp"
#include "protocol/rtmp/rtmp_message/command_message/rtmp_connect_resp_message.hpp"
#include "protocol/rtmp/rtmp_message/command_message/rtmp_create_stream_message.hpp"
#include "protocol/rtmp/rtmp_message/command_message/rtmp_create_stream_resp_message.hpp"
#include "protocol/rtmp/rtmp_message/command_message/rtmp_fcpublish_message.hpp"
#include "protocol/rtmp/rtmp_message/command_message/rtmp_fcpublish_resp_message.hpp"
#include "protocol/rtmp/rtmp_message/command_message/rtmp_onstatus_message.hpp"
#include "protocol/rtmp/rtmp_message/command_message/rtmp_play_message.hpp"
#include "protocol/rtmp/rtmp_message/command_message/rtmp_publish_message.hpp"
#include "protocol/rtmp/rtmp_message/command_message/rtmp_release_stream_message.hpp"
#include "protocol/rtmp/rtmp_message/command_message/rtmp_release_stream_resp_message.hpp"
#include "protocol/rtmp/rtmp_message/command_message/rtmp_set_peer_bandwidth_message.hpp"
#include "protocol/rtmp/rtmp_message/command_message/rtmp_window_ack_size_message.hpp"
#include "protocol/rtmp/rtmp_message/command_message/user_ctrl_message/stream_begin_message.hpp"
#include "protocol/rtmp/rtmp_message/data_message/rtmp_metadata_message.hpp"
#include "service/dns/dns_service.hpp"

using namespace mms;
RtmpPlayClientSession::RtmpPlayClientSession(std::shared_ptr<PublishApp> app, ThreadWorker *worker,
                                             const std::string &domain_name, const std::string &app_name,
                                             const std::string &stream_name)
    : StreamSession(worker),
      rtmp_msgs_channel_(get_worker()->get_io_context(), 1024),
      check_closable_timer_(worker->get_io_context()),
      wg_(worker) {
    app_ = app;
    set_session_type("rtmp");
    set_session_info(domain_name, app_name, stream_name);
}

RtmpPlayClientSession::~RtmpPlayClientSession() { CORE_DEBUG("destroy RtmpPlayClientSession"); }

void RtmpPlayClientSession::on_socket_open(std::shared_ptr<SocketInterface> sock) { (void)sock; }

void RtmpPlayClientSession::on_socket_close(std::shared_ptr<SocketInterface> sock) { (void)sock; }

void RtmpPlayClientSession::service() {
    auto self(std::static_pointer_cast<RtmpPlayClientSession>(shared_from_this()));
    auto media_source =
        SourceManager::get_instance().get_source(get_domain_name(), get_app_name(), get_stream_name());
    if (media_source) {
        return;
    }

    auto publish_app = std::static_pointer_cast<PublishApp>(app_);
    rtmp_media_source_ = std::make_shared<RtmpMediaSource>(get_worker(), std::weak_ptr<StreamSession>(self), publish_app);
    rtmp_media_source_->set_origin(true);
    rtmp_media_source_->set_session(self);
    rtmp_media_source_->set_source_info(get_domain_name(), get_app_name(), get_stream_name());

    if (!SourceManager::get_instance().add_source(get_domain_name(), get_app_name(), get_stream_name(),
                                                  rtmp_media_source_)) {
        return;
    }

    // 根据url获取信息
    std::string protocol;
    std::string domain;
    uint16_t port;
    std::string path;
    std::unordered_map<std::string, std::string> params;
    if (!Utils::parse_url(url_, protocol, domain, port, path, params)) {  // todo:add log
        CORE_ERROR("parse url:{} failed", url_);
        return;
    }

    if (protocol == "rtmp") {
        conn_ = std::make_shared<TcpSocket>(this, worker_);
    } else if (protocol == "rtmps") {
        conn_ = std::make_shared<TlsSocket>(this, worker_);
    } else {
        close();
        return;
    }

    handshake_ = std::make_unique<RtmpHandshake>(conn_);
    chunk_protocol_ = std::make_unique<RtmpChunkProtocol>(conn_);

    wg_.add(1);
    boost::asio::co_spawn(
        worker_->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            boost::system::error_code ec;
            auto publish_app = std::static_pointer_cast<PublishApp>(app_);
            while (1) {
                check_closable_timer_.expires_after(
                    std::chrono::milliseconds(pull_config_->no_players_timeout_ms() / 2));  // 10s检查一次
                co_await check_closable_timer_.async_wait(
                    boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                if (boost::asio::error::operation_aborted == ec) {
                    break;
                }

                if (!rtmp_media_source_) {
                    break;
                }

                if (rtmp_media_source_->has_no_sinks_for_time(
                        pull_config_->no_players_timeout_ms())) {  // 已经30秒没人播放了
                    CORE_DEBUG("close RtmpPlayClientSession because no players for 10s");
                    break;
                }
            }
            co_return;
        },
        [this, self](std::exception_ptr exp) {
            (void)exp;
            wg_.done();
            close();
        });

    wg_.add(1);
    boost::asio::co_spawn(
        conn_->get_worker()->get_io_context(),
        [this, self, domain, path, port]() -> boost::asio::awaitable<void> {
            // 解析app名称
            std::vector<std::string> vs;
            boost::split(vs, path, boost::is_any_of("/"));
            if (vs.size() <= 2) {
                co_return;
            }

            // 获取域名ip
            auto ip = DnsService::get_instance().get_ip(domain);
            if (!ip) {
                CORE_ERROR("could not find ip for:%s", domain.c_str());
                co_return;
            }
            auto &server_ip = ip.value();
            // 连接
            if (!co_await conn_->connect(server_ip, port)) {
                CORE_ERROR("rtmp:connect to {}:{} failed", server_ip, port);
                co_return;
            }
            rtmp_media_source_->set_client_ip(server_ip);
            
            // 启动握手
            if (!co_await handshake_->do_client_handshake()) {
                CORE_ERROR("rtmp:do_client_handshake failed");
                co_return;
            }

            wg_.add(1);
            // 启动发送协程
            boost::asio::co_spawn(
                conn_->get_worker()->get_io_context(),
                [this, self]() -> boost::asio::awaitable<void> {
                    boost::system::error_code ec;
                    while (1) {
                        auto [rtmp_msgs, ch] = co_await rtmp_msgs_channel_.async_receive(
                            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                        if (ec) {
                            break;
                        }

                        if (!(co_await chunk_protocol_->send_rtmp_messages(rtmp_msgs))) {
                            if (ch) {
                                ch->close();
                            }
                            CORE_ERROR("send rtmp message failed");
                            co_return;
                        }

                        if (ch) {
                            ch->close();
                        }
                    }
                    co_return;
                },
                [this](std::exception_ptr exp) {
                    (void)exp;
                    wg_.done();
                    close();
                    CORE_DEBUG("RtmpPlayClientSession send coroutine exited");
                });

            RtmpSetChunkSizeMessage set_chunk_size_msg(40960);
            if (!co_await send_rtmp_message(set_chunk_size_msg)) {
                co_return;
            }
            chunk_protocol_->set_out_chunk_size(40960);

            auto slash_pos = url_.find_last_of("/");
            auto tc_url = url_.substr(0, slash_pos);
            auto stream_and_params = url_.substr(slash_pos + 1);
            RtmpConnectCommandMessage connect_command_msg(transaction_id_++, tc_url, "", "", get_app_name());
            if (!co_await send_rtmp_message(connect_command_msg)) {
                co_return;
            }

            RtmpCreateStreamMessage create_msg(transaction_id_++);
            if (!co_await send_rtmp_message(create_msg)) {
                CORE_ERROR("send create stream msg failed");
                co_return;
            }

            RtmpPlayMessage play_msg(transaction_id_++, stream_and_params);
            if (!co_await send_rtmp_message(play_msg)) {
                co_return;
            }

            // 循环接收chunk层收到的rtmp消息
            co_await chunk_protocol_->cycle_recv_rtmp_message(
                std::bind(&RtmpPlayClientSession::on_recv_rtmp_message, this, std::placeholders::_1));
            co_return;
        },
        [this, self](std::exception_ptr exp) {
            (void)exp;
            wg_.done();
            close();
            CORE_DEBUG("RtmpPlayClientSession recv coroutine exited");
        });
}

void RtmpPlayClientSession::update_active_timestamp() { last_active_time_ = Utils::get_current_ms(); }

void RtmpPlayClientSession::close() {
    // todo: how to record 404 error to log.
    if (closed_.test_and_set(std::memory_order_acquire)) {
        return;
    }

    auto self(this->shared_from_this());
    boost::asio::co_spawn(
        conn_->get_worker()->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            CORE_DEBUG("closing RtmpPlayClientSession...");
            check_closable_timer_.cancel();
            if (rtmp_msgs_channel_.is_open()) {  // 结束发送协程
                rtmp_msgs_channel_.close();
            }

            if (conn_) {
                conn_->close();
            }

            co_await wg_.wait();

        if (rtmp_media_source_) {// 如果是推流的session
            auto publish_app = rtmp_media_source_->get_app();
            rtmp_media_source_->set_session(nullptr);//解除绑定
            start_delayed_source_check_and_delete(publish_app->get_conf()->get_stream_resume_timeout(), rtmp_media_source_);
            co_await publish_app->on_unpublish(std::static_pointer_cast<StreamSession>(shared_from_this()));
            rtmp_media_source_ = nullptr;
        }

        if (conn_) {
            conn_.reset();
        }
        CORE_DEBUG("RtmpPlayClientSession closed");
        co_return;
    }, boost::asio::detached);
}

void RtmpPlayClientSession::set_url(const std::string &url) { url_ = url; }

std::shared_ptr<RtmpMediaSource> RtmpPlayClientSession::get_rtmp_media_source() { return rtmp_media_source_; }

boost::asio::awaitable<bool> RtmpPlayClientSession::send_acknowledge_msg_if_required() {
    int64_t delta = chunk_protocol_->get_last_ack_bytes() - conn_->get_in_bytes();
    if (delta >= chunk_protocol_->get_in_window_acknowledge_size() / 2) {  // acknowledge
        RtmpAcknwledgeMessage ack_msg(conn_->get_in_bytes());
        if (!co_await send_rtmp_message(ack_msg)) {
            co_return false;
        }
        chunk_protocol_->set_last_ack_bytes(conn_->get_in_bytes());
        co_return true;
    }
    co_return true;
}

boost::asio::awaitable<bool> RtmpPlayClientSession::on_recv_rtmp_message(
    std::shared_ptr<RtmpMessage> rtmp_msg) {
    if (!co_await send_acknowledge_msg_if_required()) {
        co_return false;
    }

    if (chunk_protocol_->is_protocol_control_message(rtmp_msg)) {
        if (!co_await chunk_protocol_->handle_protocol_control_message(rtmp_msg)) {
            co_return false;
        } else {
            co_return true;
        }
    }

    switch (rtmp_msg->get_message_type()) {
        case RTMP_MESSAGE_TYPE_AUDIO: {
            co_return handle_audio_msg(rtmp_msg);
        }
        case RTMP_MESSAGE_TYPE_VIDEO: {
            co_return handle_video_msg(rtmp_msg);
        }
        case RTMP_MESSAGE_TYPE_AMF0_COMMAND: {
            co_return co_await handle_amf0_command(rtmp_msg);
        }
        case RTMP_MESSAGE_TYPE_AMF0_DATA: {
            co_return handle_amf0_data(rtmp_msg);
        }
        case RTMP_MESSAGE_TYPE_USER_CONTROL: {
            co_return handle_user_control_msg(rtmp_msg);
        }
        case RTMP_CHUNK_MESSAGE_TYPE_ACKNOWLEDGEMENT: {
            co_return handle_acknowledgement(rtmp_msg);
        }
        case RTMP_CHUNK_MESSAGE_TYPE_SET_PEER_BANDWIDTH: {
            co_return true;
        }
        default: {
        }
    }
    co_return true;
}
// 异步方式发送rtmp消息
boost::asio::awaitable<bool> RtmpPlayClientSession::send_rtmp_message(
    const std::vector<std::shared_ptr<RtmpMessage>> &rtmp_msgs) {
    co_await rtmp_msgs_channel_.async_send(boost::system::error_code{}, rtmp_msgs, nullptr,
                                           boost::asio::use_awaitable);
    co_return true;
}

boost::asio::awaitable<bool> RtmpPlayClientSession::handle_amf0_command(
    std::shared_ptr<RtmpMessage> rtmp_msg) {
    Amf0String command_name;
    auto payload = rtmp_msg->get_using_data();
    int32_t consumed = command_name.decode((uint8_t *)payload.data(), payload.size());
    if (consumed < 0) {
        co_return false;
    }

    auto name = command_name.get_value();
    if (name == "_result") {
        co_return co_await handle_amf0_result_command(rtmp_msg);
    } else if (name == "onStatus") {
        co_return co_await handle_amf0_status_command(rtmp_msg);
    }

    co_return true;
}

boost::asio::awaitable<bool> RtmpPlayClientSession::handle_amf0_status_command(
    std::shared_ptr<RtmpMessage> rtmp_msg) {
    RtmpOnStatusMessage status_command;
    auto consumed = status_command.decode(rtmp_msg);
    if (consumed < 0) {
        co_return false;
    }

    auto &info = status_command.info();
    auto code = info.get_property<Amf0String>("code");
    if (!code) {
        co_return false;
    }

    if (code.value() == RTMP_STATUS_STREAM_PLAY_START) {
        rtmp_media_source_->set_status(E_SOURCE_STATUS_OK);
        // 通知app开始播放
        auto publish_app = std::static_pointer_cast<PublishApp>(app_);
        auto self(shared_from_this());
        auto err = co_await publish_app->on_publish(std::static_pointer_cast<StreamSession>(self));
        if (err.code != 0) {
            co_return false;
        }
        co_return true;
    } else if (code.value() == RTMP_STATUS_STREAM_NOT_FOUND) {
        rtmp_media_source_->set_status(E_SOURCE_STATUS_NOT_FOUND);
        co_return false;
    } else if (code.value() == RTMP_RESULT_CONNECT_REJECTED) {
        rtmp_media_source_->set_status(E_SOURCE_STATUS_FORBIDDEN);
        co_return false;
    } 

    co_return false;
}

boost::asio::awaitable<bool> RtmpPlayClientSession::handle_amf0_result_command(
    std::shared_ptr<RtmpMessage> rtmp_msg) {
    (void)rtmp_msg;
    co_return true;
}

bool RtmpPlayClientSession::handle_video_msg(std::shared_ptr<RtmpMessage> msg) {
    return rtmp_media_source_->on_video_packet(msg);
}

bool RtmpPlayClientSession::handle_audio_msg(std::shared_ptr<RtmpMessage> msg) {
    return rtmp_media_source_->on_audio_packet(msg);
}

bool RtmpPlayClientSession::handle_amf0_data(std::shared_ptr<RtmpMessage> rtmp_msg) {  // usually is metadata
    Amf0String command_name;
    auto payload = rtmp_msg->get_using_data();
    int32_t consumed = command_name.decode((uint8_t *)payload.data(), payload.size());
    if (consumed < 0) {
        return false;
    }

    auto name = command_name.get_value();
    if (name == "onMetaData" || name == "@setDataFrame") {
        return rtmp_media_source_->on_metadata(rtmp_msg);
    }
    return true;
}

bool RtmpPlayClientSession::handle_acknowledgement(std::shared_ptr<RtmpMessage> rtmp_msg) {
    // todo
    // nothing to do
    (void)rtmp_msg;
    return true;
}

bool RtmpPlayClientSession::handle_user_control_msg(std::shared_ptr<RtmpMessage> rtmp_msg) {
    // todo handle user control msg
    (void)rtmp_msg;
    return true;
}