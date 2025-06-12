#include "rtmp_server_session.hpp"

#include <boost/algorithm/string.hpp>

#include "base/network/socket_interface.hpp"
#include "core/rtmp_media_sink.hpp"
#include "core/rtmp_media_source.hpp"
#include "core/source_manager.hpp"
#include "server/session.hpp"

// #include "core/media_center/media_center_manager.h"

#include "app/app.h"
#include "app/play_app.h"
#include "app/publish_app.h"
#include "bridge/media_bridge.hpp"
#include "config/app_config.h"
#include "config/config.h"
#include "protocol/rtmp/amf0/amf0_number.hpp"
#include "protocol/rtmp/amf0/amf0_object.hpp"
#include "protocol/rtmp/amf0/amf0_string.hpp"
#include "protocol/rtmp/rtmp_message/chunk_message/rtmp_acknowledge_message.hpp"
#include "protocol/rtmp/rtmp_message/chunk_message/rtmp_window_acknowledge_size_message.hpp"

#include "base/network/bitrate_monitor.h"

using namespace mms;
RtmpServerSession::RtmpServerSession(std::shared_ptr<SocketInterface> conn)
    : StreamSession(conn->get_worker()),
      conn_(conn),
      handshake_(conn),
      chunk_protocol_(conn),
      rtmp_msgs_channel_(get_worker()->get_io_context(), 1024),
      alive_timeout_timer_(get_worker()->get_io_context()),
      statistic_timer_(get_worker()->get_io_context()),
      wg_(get_worker()) {
    set_session_type("rtmp");
}

RtmpServerSession::~RtmpServerSession() { CORE_DEBUG("destroy RtmpServerSession {}", get_session_name()); }

void RtmpServerSession::start_alive_checker() {
    auto self(shared_from_this());
    wg_.add(1);
    boost::asio::co_spawn(
        worker_->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            boost::system::error_code ec;
            while (1) {
                alive_timeout_timer_.expires_after(std::chrono::seconds(10));
                co_await alive_timeout_timer_.async_wait(
                    boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                if (ec) {
                    co_return;
                }

                int64_t now_ms = Utils::get_current_ms();
                if (now_ms - last_active_time_ >= 50000) {  // 5秒没数据，超时
                    CORE_WARN("rtmp session:{} is not alive, last_active_time:{}", get_session_name(),
                              last_active_time_);
                    co_return;
                }
            }
            co_return;
        },
        [this, self](std::exception_ptr exp) {
            (void)exp;
            close();
            wg_.done();
        });
}

void RtmpServerSession::start_statistic_timer() {
    auto self(shared_from_this());
    wg_.add(1);
    boost::asio::co_spawn(
        worker_->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            boost::system::error_code ec;
            while (1) {
                statistic_timer_.expires_after(std::chrono::seconds(10));
                co_await statistic_timer_.async_wait(
                    boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                if (ec) {
                    co_return;
                }
                video_bitrate_monitor_->tick();
                audio_bitrate_monitor_->tick();
            }
            co_return;
        },
        [this, self](std::exception_ptr exp) {
            (void)exp;
            close();
            wg_.done();
        });
}

void RtmpServerSession::update_active_timestamp() { last_active_time_ = Utils::get_current_ms(); }

void RtmpServerSession::start_send_coroutine() {
    // 启动发送协程
    auto self(shared_from_this());
    wg_.add(1);
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

                update_active_timestamp();
                if (!(co_await chunk_protocol_.send_rtmp_messages(rtmp_msgs))) {
                    if (ch) {
                        ch->close();
                    }
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
            close();
            wg_.done();
        });
}

void RtmpServerSession::start_recv_coroutine() {
    auto self(shared_from_this());
    wg_.add(1);
    boost::asio::co_spawn(
        conn_->get_worker()->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            // 启动握手
            if (!co_await handshake_.do_server_handshake()) {
                co_return;
            }
            start_send_coroutine();
            // 发送set chunk size命令
            chunk_protocol_.set_out_chunk_size(Config::get_instance()->get_rtmp_config().out_chunk_size());
            RtmpSetChunkSizeMessage set_chunk_size_msg(
                chunk_protocol_.get_out_chunk_size());  // todo set out chunk size in conf
            if (!co_await send_rtmp_message(set_chunk_size_msg)) {
                co_return;
            }
            // 循环接收chunk层收到的rtmp消息
            co_await chunk_protocol_.cycle_recv_rtmp_message(
                std::bind(&RtmpServerSession::on_recv_rtmp_message, this, std::placeholders::_1));
            co_return;
        },
        [this, self](std::exception_ptr exp) {
            (void)exp;
            close();
            wg_.done();
        });
}

void RtmpServerSession::service() {
    CORE_DEBUG("RtmpServerSession start service");
    start_recv_coroutine();
}

void RtmpServerSession::close() {
    if (closed_.test_and_set(std::memory_order_acquire)) {
        return;
    }

    auto self(this->shared_from_this());
    boost::asio::co_spawn(
        conn_->get_worker()->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            CORE_DEBUG("closing RtmpServerSession {} ...", get_session_name());
            boost::system::error_code ec;
            if (conn_) {
                conn_->close();
            }

            if (rtmp_msgs_channel_.is_open()) {  // 结束发送协程
                rtmp_msgs_channel_.close();
            }

            alive_timeout_timer_.cancel();
            statistic_timer_.cancel();
            co_await wg_.wait();

            if (rtmp_media_sink_) {  // 如果是播放的session
                auto play_app = std::static_pointer_cast<PlayApp>(get_app());
                std::string source_name;
                if (play_app) {
                    auto publish_app = play_app->get_publish_app();
                    source_name = publish_app->get_domain_name() + "/" + app_name_ + "/" + stream_name_;
                    auto s = SourceManager::get_instance().get_source(get_domain_name(), get_app_name(),
                                                                      get_stream_name());
                    if (s) {
                        s->remove_media_sink(rtmp_media_sink_);
                    }
                }

                rtmp_media_sink_->on_rtmp_message({});
                rtmp_media_sink_->on_close({});
                rtmp_media_sink_->close();
                rtmp_media_sink_ = nullptr;
            }

            if (rtmp_media_source_) {  // 如果是推流的session
                auto publish_app = rtmp_media_source_->get_app();
                rtmp_media_source_->set_session(nullptr);  // 解除绑定
                start_delayed_source_check_and_delete(publish_app->get_conf()->get_stream_resume_timeout(),
                                                      rtmp_media_source_);
                co_await publish_app->on_unpublish(
                    std::static_pointer_cast<StreamSession>(shared_from_this()));
            }

            if (conn_) {
                conn_.reset();
            }

            CORE_DEBUG("close RtmpServerSession {} done", get_session_name());
            co_return;
        },
        boost::asio::detached);
}

boost::asio::awaitable<bool> RtmpServerSession::send_acknowledge_msg_if_required() {
    int64_t delta = chunk_protocol_.get_last_ack_bytes() - conn_->get_in_bytes();
    if (delta >= chunk_protocol_.get_in_window_acknowledge_size() / 2) {  // acknowledge
        RtmpAcknwledgeMessage ack_msg(conn_->get_in_bytes());
        if (!co_await send_rtmp_message(ack_msg)) {
            co_return false;
        }
        chunk_protocol_.set_last_ack_bytes(conn_->get_in_bytes());
        co_return true;
    }
    co_return true;
}

boost::asio::awaitable<bool> RtmpServerSession::on_recv_rtmp_message(std::shared_ptr<RtmpMessage> rtmp_msg) {
    update_active_timestamp();
    if (!co_await send_acknowledge_msg_if_required()) {
        co_return false;
    }

    if (chunk_protocol_.is_protocol_control_message(rtmp_msg)) {
        if (!co_await chunk_protocol_.handle_protocol_control_message(rtmp_msg)) {
            co_return false;
        } else {
            co_return true;
        }
    }

    switch (rtmp_msg->get_message_type()) {
        case RTMP_MESSAGE_TYPE_AMF0_DATA: {
            co_return handle_amf0_data(rtmp_msg);
        }
        case RTMP_MESSAGE_TYPE_AUDIO: {
            co_return handle_audio_msg(rtmp_msg);
        }
        case RTMP_MESSAGE_TYPE_VIDEO: {
            co_return handle_video_msg(rtmp_msg);
        }
        case RTMP_MESSAGE_TYPE_AMF0_COMMAND: {
            co_return co_await handle_amf0_command(rtmp_msg);
        }
        case RTMP_MESSAGE_TYPE_USER_CONTROL: {
            co_return handle_user_control_msg(rtmp_msg);
        }
        case RTMP_CHUNK_MESSAGE_TYPE_ACKNOWLEDGEMENT: {
            co_return handle_acknowledgement(rtmp_msg);
        }
        default: {
            co_return true;
        }
    }
    co_return true;
}

// 异步方式发送rtmp消息
boost::asio::awaitable<bool> RtmpServerSession::send_rtmp_message(
    const std::vector<std::shared_ptr<RtmpMessage>>& rtmp_msgs) {
    co_await rtmp_msgs_channel_.async_send(boost::system::error_code{}, rtmp_msgs, nullptr,
                                           boost::asio::use_awaitable);
    co_return true;
}

boost::asio::awaitable<bool> RtmpServerSession::handle_amf0_command(std::shared_ptr<RtmpMessage> rtmp_msg) {
    Amf0String command_name;
    auto payload = rtmp_msg->get_using_data();
    int32_t consumed = command_name.decode((uint8_t*)payload.data(), payload.size());
    if (consumed < 0) {
        CORE_ERROR("decode rtmp command msg failed, code:{}", consumed);
        co_return false;
    }

    auto name = command_name.get_value();
    if (name == "connect") {
        co_return co_await handle_amf0_connect_command(rtmp_msg);
    } else if (name == "releaseStream") {
        co_return co_await handle_amf0_release_stream_command(rtmp_msg);
    } else if (name == "FCPublish") {
        co_return co_await handle_amf0_fcpublish_command(rtmp_msg);
    } else if (name == "createStream") {
        co_return co_await handle_amf0_createstream_command(rtmp_msg);
    } else if (name == "publish") {
        co_return co_await handle_amf0_publish_command(rtmp_msg);
    } else if (name == "play") {
        co_return co_await handle_amf0_play_command(rtmp_msg);
    } else if (name.empty()) {
        co_return false;
    }

    co_return true;
}

boost::asio::awaitable<bool> RtmpServerSession::handle_amf0_connect_command(
    std::shared_ptr<RtmpMessage> rtmp_msg) {
    RtmpConnectCommandMessage connect_command;
    auto consumed = connect_command.decode(rtmp_msg);
    if (consumed < 0) {
        co_return false;
    }

    // 获取domain, app, stream_name信息
    if (!parse_connect_cmd(connect_command)) {
        co_return false;
    }

    // send window ack size to client
    RtmpWindowAckSizeMessage window_ack_size_msg(window_ack_size_);
    if (!co_await send_rtmp_message(window_ack_size_msg)) {
        co_return false;
    }

    RtmpSetPeerBandwidthMessage set_peer_bandwidth_msg(800000000, LIMIT_TYPE_DYNAMIC);
    if (!co_await send_rtmp_message(set_peer_bandwidth_msg)) {
        co_return false;
    }

    RtmpConnectRespMessage result_msg(connect_command, "_result");
    result_msg.props().set_item_value("fmsVer", "FMS/3,0,1,123");
    result_msg.props().set_item_value("capabilities", 31);

    result_msg.info().set_item_value("level", "status");
    result_msg.info().set_item_value("code", RTMP_RESULT_CONNECT_SUCCESS);
    result_msg.info().set_item_value("description", "Connection succeed.");
    result_msg.info().set_item_value("objEncoding",
                                     connect_command.object_encoding_);  // todo objencoding需要自己判断
    if (!co_await send_rtmp_message(result_msg)) {
        co_return false;
    }
    co_return true;
}

boost::asio::awaitable<bool> RtmpServerSession::handle_amf0_release_stream_command(
    std::shared_ptr<RtmpMessage> rtmp_msg) {
    RtmpReleaseStreamMessage release_command;
    auto consumed = release_command.decode(rtmp_msg);
    if (consumed < 0) {
        co_return false;
    }

    RtmpReleaseStreamRespMessage result_msg(release_command, "_result");
    if (!co_await send_rtmp_message(result_msg)) {
        co_return false;
    }
    co_return true;
}

boost::asio::awaitable<bool> RtmpServerSession::handle_amf0_fcpublish_command(
    std::shared_ptr<RtmpMessage> rtmp_msg) {
    RtmpFCPublishMessage fcpublish_cmd;
    auto consumed = fcpublish_cmd.decode(rtmp_msg);
    if (consumed < 0) {
        co_return false;
    }

    RtmpFCPublishRespMessage result_msg(fcpublish_cmd, "_result");
    if (!co_await send_rtmp_message(result_msg)) {
        co_return false;
    }
    co_return true;
}

boost::asio::awaitable<bool> RtmpServerSession::handle_amf0_createstream_command(
    std::shared_ptr<RtmpMessage> rtmp_msg) {
    RtmpCreateStreamMessage create_stream_cmd;
    auto consumed = create_stream_cmd.decode(rtmp_msg);
    if (consumed < 0) {
        CORE_ERROR("handle_amf0_createstream_command decoe failed");
        co_return false;
    }

    RtmpCreateStreamRespMessage result_msg(create_stream_cmd, "_result");
    if (!co_await send_rtmp_message(result_msg)) {
        CORE_ERROR("handle_amf0_createstream_command failed");
        co_return false;
    }
    co_return true;
}

boost::asio::awaitable<bool> RtmpServerSession::handle_amf0_publish_command(
    std::shared_ptr<RtmpMessage> rtmp_msg) {
    auto self(std::static_pointer_cast<RtmpServerSession>(shared_from_this()));
    RtmpPublishMessage publish_cmd;
    auto consumed = publish_cmd.decode(rtmp_msg);
    if (consumed < 0) {
        co_return false;
    }

    if (!parse_publish_cmd(publish_cmd)) {
        co_return false;
    }

    if (!find_and_set_app(domain_name_, app_name_, true)) {
        CORE_ERROR("could not find conf for domain:{}, app:{}", domain_name_, app_name_);
        co_return false;
    }

    is_publisher_ = true;
    is_player_ = false;
    auto rtmp_media_source =
        SourceManager::get_instance().get_source(get_domain_name(), get_app_name(), get_stream_name());
    if (!rtmp_media_source || rtmp_media_source->get_media_type() != "rtmp") {
        rtmp_media_source_ = std::make_shared<RtmpMediaSource>(
            get_worker(), std::weak_ptr<StreamSession>(self), std::static_pointer_cast<PublishApp>(app_));
    } else {
        auto old_session = std::static_pointer_cast<RtmpServerSession>(rtmp_media_source->get_session());
        if (old_session) {
            old_session->detach_source();
            old_session->close();  // 关闭老的推流端
        }
        rtmp_media_source_ = std::static_pointer_cast<RtmpMediaSource>(rtmp_media_source);
    }

    if (!rtmp_media_source_) {
        CORE_ERROR("create rtmp media source failed");
        co_return false;
    }

    rtmp_media_source_->set_origin(true);
    rtmp_media_source_->set_session(self);
    rtmp_media_source_->set_source_info(domain_name_, app_name_, stream_name_);
    rtmp_media_source_->set_client_ip(conn_->get_remote_address());
    // 通知app开始播放
    auto publish_app = std::static_pointer_cast<PublishApp>(app_);
    auto err = co_await publish_app->on_publish(std::static_pointer_cast<StreamSession>(self));
    if (err.code != 0) {
        co_return false;
    }

    RtmpOnStatusMessage status_msg;
    status_msg.info().set_item_value("level", "status");
    status_msg.info().set_item_value("code", RTMP_STATUS_STREAM_PUBLISH_START);
    status_msg.info().set_item_value("description", "publish start ok.");
    status_msg.info().set_item_value("clientid", "mms");
    if (!co_await send_rtmp_message(status_msg)) {
        co_return false;
    }

    spdlog::info("add source:{}", get_session_name());
    auto ret = SourceManager::get_instance().add_source(get_domain_name(), get_app_name(), get_stream_name(),
                                                        rtmp_media_source_);
    if (!ret) {
        rtmp_media_source_->close();
        co_return false;
    }
    rtmp_media_source_->set_status(E_SOURCE_STATUS_OK);
    publish_app->on_create_source(get_domain_name(), get_app_name(), get_stream_name(), rtmp_media_source_);
    start_alive_checker();
    co_return true;
}

void RtmpServerSession::detach_source() { rtmp_media_source_ = nullptr; }

boost::asio::awaitable<bool> RtmpServerSession::handle_amf0_play_command(
    std::shared_ptr<RtmpMessage> rtmp_msg) {
    RtmpPlayMessage play_cmd;
    auto consumed = play_cmd.decode(rtmp_msg);
    if (consumed < 0) {
        CORE_ERROR("parse play cmd failed, consumed:{}", consumed);
        co_return false;
    }

    if (!parse_play_cmd(play_cmd)) {
        CORE_ERROR("parse play cmd failed");
        co_return false;
    }

    if (!find_and_set_app(domain_name_, app_name_, false)) {
        CORE_ERROR("could not find conf for domain:{}, app:{}", domain_name_, app_name_);
        co_return false;
    }

    auto play_app = std::static_pointer_cast<PlayApp>(get_app());
    if (!play_app) {
        co_return false;
    }

    auto self(std::static_pointer_cast<RtmpServerSession>(shared_from_this()));
    // 1. 判断是否需要鉴权并进行鉴权
    auto err = co_await play_app->on_play(self);
    if (err.code != 0) {
        CORE_ERROR("play auth check failed, err:{}", err.msg);
        co_return false;
    }
    // 2. 获取源
    auto publish_app = play_app->get_publish_app();
    // todo: how to record 404 error to log.
    // auto source_name = publish_app->get_domain_name() + "/" + app_name_ + "/" + stream_name_;
    auto source = SourceManager::get_instance().get_source(publish_app->get_domain_name(), get_app_name(),
                                                           get_stream_name());
    if (!source) {  // 2.本地配置查找外部回源
        source = co_await publish_app->find_media_source(self);
    }
    // 3.到media center中心查找
    // todo
    // if (!source) {
    //     source = co_await MediaCenterManager::get_instance().find_and_create_pull_media_source(self);
    // }

    if (!source) {
        CORE_ERROR("no source found:{}/{}/{}", publish_app->get_domain_name(), get_app_name(),
                   get_stream_name());
        RtmpOnStatusMessage status_msg;
        status_msg.info().set_item_value("level", "status");
        status_msg.info().set_item_value("code", RTMP_STATUS_STREAM_NOT_FOUND);
        status_msg.info().set_item_value("description", "not found");
        status_msg.info().set_item_value("clientid", "mms");
        if (!co_await send_rtmp_message(status_msg)) {
            co_return false;
        }
        co_return false;
    }

    // 4. 判断是否需要进行桥转换
    std::shared_ptr<MediaBridge> bridge;
    if (source->get_media_type() != "rtmp") {
        bridge = source->get_or_create_bridge(source->get_media_type() + "-rtmp", publish_app, stream_name_);
        if (!bridge) {  // todo:发送FORBIDDEN
            CORE_ERROR("could not create bridge:{}", source->get_media_type() + "-rtmp");
            RtmpOnStatusMessage status_msg;
            status_msg.info().set_item_value("level", "status");
            status_msg.info().set_item_value("code", RTMP_RESULT_CONNECT_REJECTED);
            status_msg.info().set_item_value("description", "forbidden");
            status_msg.info().set_item_value("clientid", "mms");
            if (!co_await send_rtmp_message(status_msg)) {
                co_return false;
            }
            co_return false;
        }
        source = bridge->get_media_source();
    }
    // 5. 发送正常播放消息
    RtmpStreamBeginMessage stream_begin_msg(1);  // 只用1这个stream_id
    if (!co_await send_rtmp_message(stream_begin_msg)) {
        co_return false;
    }

    RtmpOnStatusMessage status_msg;
    status_msg.info().set_item_value("level", "status");
    status_msg.info().set_item_value("code", RTMP_STATUS_STREAM_PLAY_START);
    status_msg.info().set_item_value("description", "play start ok.");
    status_msg.info().set_item_value("clientid", "mms");
    if (!co_await send_rtmp_message(status_msg)) {
        co_return false;
    }

    RtmpAccessSampleMessage access_sample_msg;
    if (!co_await send_rtmp_message(access_sample_msg)) {
        co_return false;
    }

    is_publisher_ = false;
    is_player_ = true;
    rtmp_media_sink_ = std::make_shared<RtmpMediaSink>(get_worker());
    rtmp_media_sink_->on_close([this, self]() { close(); });

    rtmp_media_sink_->set_on_source_status_changed_cb(
        [this, self, source](SourceStatus status) -> boost::asio::awaitable<void> {
            if (status == E_SOURCE_STATUS_OK) {
                rtmp_media_sink_->on_rtmp_message(
                    [this, self](const std::vector<std::shared_ptr<RtmpMessage>>& rtmp_msgs)
                        -> boost::asio::awaitable<bool> { co_return co_await send_rtmp_message(rtmp_msgs); });
            } else if (status == E_SOURCE_STATUS_NOT_FOUND) {
                RtmpOnStatusMessage status_msg;
                status_msg.info().set_item_value("level", "status");
                status_msg.info().set_item_value("code", RTMP_STATUS_STREAM_NOT_FOUND);
                status_msg.info().set_item_value("description", "not found");
                status_msg.info().set_item_value("clientid", "mms");
                if (!co_await send_rtmp_message(status_msg)) {
                    co_return;
                }
            } else if (status == E_SOURCE_STATUS_FORBIDDEN || status == E_SOURCE_STATUS_UNAUTHORIZED) {
                RtmpOnStatusMessage status_msg;
                status_msg.info().set_item_value("level", "status");
                status_msg.info().set_item_value("code", RTMP_RESULT_CONNECT_REJECTED);
                status_msg.info().set_item_value("description", "forbidden");
                status_msg.info().set_item_value("clientid", "mms");
                if (!co_await send_rtmp_message(status_msg)) {
                    co_return;
                }
            } else {
                RtmpOnStatusMessage status_msg;
                status_msg.info().set_item_value("level", "status");
                status_msg.info().set_item_value("code", RTMP_STATUS_STREAM_PLAY_FAILED);
                status_msg.info().set_item_value("description", "play failed");
                status_msg.info().set_item_value("clientid", "mms");
                if (!co_await send_rtmp_message(status_msg)) {
                    co_return;
                }
            }
            co_return;
        });

    bool ret = true;
    ret = source->add_media_sink(rtmp_media_sink_);
    co_return ret;
}

bool RtmpServerSession::handle_video_msg(std::shared_ptr<RtmpMessage> msg) {
    if (!rtmp_media_source_) {
        return false;
    }
    video_bitrate_monitor_->on_bytes_in(msg->get_using_data().size());
    return rtmp_media_source_->on_video_packet(msg);
}

bool RtmpServerSession::handle_audio_msg(std::shared_ptr<RtmpMessage> msg) {
    if (!rtmp_media_source_) {
        return false;
    }
    audio_bitrate_monitor_->on_bytes_in(msg->get_using_data().size());
    return rtmp_media_source_->on_audio_packet(msg);
}

bool RtmpServerSession::handle_amf0_data(std::shared_ptr<RtmpMessage> rtmp_msg) {  // usually is metadata
    Amf0String command_name;
    auto payload = rtmp_msg->get_using_data();
    int32_t consumed = command_name.decode((uint8_t*)payload.data(), payload.size());
    if (consumed < 0) {
        return false;
    }

    auto name = command_name.get_value();
    if (name == "onMetaData" || name == "@setDataFrame") {
        if (!rtmp_media_source_) {
            return false;
        }
        return rtmp_media_source_->on_metadata(rtmp_msg);
    }
    return true;
}

bool RtmpServerSession::handle_acknowledgement(std::shared_ptr<RtmpMessage> rtmp_msg) {
    // todo
    // nothing to do
    (void)rtmp_msg;
    return true;
}

bool RtmpServerSession::handle_user_control_msg(std::shared_ptr<RtmpMessage> rtmp_msg) {
    // todo handle user control msg
    (void)rtmp_msg;
    return true;
}

bool RtmpServerSession::parse_connect_cmd(RtmpConnectCommandMessage& connect_command) {
    {  // parse domain
        std::vector<std::string> vs;
        boost::split(vs, connect_command.tc_url_, boost::is_any_of("/"));
        if (vs.size() < 3) {
            return false;
        }

        if (vs[0] != "rtmp:" && vs[0] != "rtmps:") {
            return false;
        }

        if (vs[1] != "") {
            return false;
        }

        domain_name_ = vs[2];
        if (domain_name_.find(":") != std::string::npos) {  // 去掉端口号
            std::vector<std::string> tmp;
            boost::split(tmp, domain_name_, boost::is_any_of(":"));
            if (tmp.size() > 1) {
                domain_name_ = tmp[0];
            }
        }
    }

    {  // parse app, stream, params
        std::vector<std::string> vs;
        boost::split(vs, connect_command.app_, boost::is_any_of("/"));
        if (vs.size() < 1) {
            return false;
        }

        app_name_ = vs[0];
        if (vs.size() >= 2) {  // 兼容obs推流时，可能将流名写到前面
            // todo 考虑带参数的情况
            stream_name_ = vs[1];
            auto qmark_pos = stream_name_.find("?");
            if (qmark_pos != std::string::npos) {
                stream_name_ = stream_name_.substr(0, qmark_pos);
            }
        }
    }

    {
        auto question_mark_pos = connect_command.tc_url_.find("?");
        if (question_mark_pos != std::string::npos) {  // 解析错误
            std::vector<std::string> vs;
            std::string params_list = connect_command.tc_url_.substr(
                question_mark_pos + 1, connect_command.tc_url_.size() - question_mark_pos - 1);
            boost::split(vs, params_list, boost::is_any_of("&"));
            for (auto& s : vs) {
                auto equ_pos = s.find("=");
                if (equ_pos == std::string::npos) {
                    continue;
                }

                std::string name = s.substr(0, equ_pos);
                std::string value = s.substr(equ_pos + 1);
                set_param(name, value);
            }
        }
    }
    return true;
}

bool RtmpServerSession::parse_publish_cmd(RtmpPublishMessage& pub_cmd) {
    auto& stream_name = pub_cmd.stream_name();
    if (stream_name_.empty()) {
        auto question_mark_pos = stream_name.find("?");
        if (question_mark_pos != std::string::npos) {  // 解析错误
            stream_name_ = stream_name.substr(0, question_mark_pos);
            std::vector<std::string> vs;
            std::string params_list =
                stream_name.substr(question_mark_pos + 1, stream_name.size() - question_mark_pos - 1);
            boost::split(vs, params_list, boost::is_any_of("&"));
            for (auto& s : vs) {
                auto equ_pos = s.find("=");
                if (equ_pos == std::string::npos) {
                    continue;
                }

                std::string name = s.substr(0, equ_pos);
                std::string value = s.substr(equ_pos + 1);
                set_param(name, value);
            }
        } else {
            stream_name_ = stream_name;
        }

        CORE_DEBUG("stream_name:{}", stream_name_);
    }
    set_session_info(domain_name_, app_name_, stream_name_);
    return true;
}

bool RtmpServerSession::parse_play_cmd(RtmpPlayMessage& play_cmd) {
    auto& stream_name = play_cmd.stream_name();
    if (stream_name_.empty()) {
        stream_name_ = stream_name;
        auto qmark_pos = stream_name_.find("?");
        if (qmark_pos != std::string::npos) {
            stream_name_ = stream_name_.substr(0, qmark_pos);
        }
    }

    session_name_ = domain_name_ + "/" + app_name_ + "/" + stream_name_;
    return true;
}