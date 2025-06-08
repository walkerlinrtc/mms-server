#include "rtmp_publish_client_session.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>

#include "app/publish_app.h"
#include "base/network/tcp_socket.hpp"
#include "base/network/tls_socket.hpp"
#include "base/utils/utils.h"
#include "core/rtmp_media_sink.hpp"
#include "core/rtmp_media_source.hpp"
#include "core/source_manager.hpp"
#include "core/stream_session.hpp"
#include "protocol/rtmp/amf0/amf0_inc.hpp"
#include "protocol/rtmp/rtmp_chunk_protocol.hpp"
#include "protocol/rtmp/rtmp_define.hpp"
#include "protocol/rtmp/rtmp_handshake.hpp"
#include "protocol/rtmp/rtmp_message/chunk_message/rtmp_acknowledge_message.hpp"
#include "protocol/rtmp/rtmp_message/chunk_message/rtmp_set_chunk_size_message.hpp"
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
RtmpPublishClientSession::RtmpPublishClientSession(std::weak_ptr<RtmpMediaSource> source,
                                                   std::shared_ptr<PublishApp> app, ThreadWorker *worker)
    : StreamSession(worker),
      rtmp_msgs_channel_(get_worker()->get_io_context(), 1024),
      check_closable_timer_(worker->get_io_context()),
      alive_timeout_timer_(worker->get_io_context()),
      wg_(worker) {
    rtmp_source_ = source;
    app_ = app;
    rtmp_media_sink_ = std::make_shared<RtmpMediaSink>(worker_);
    set_session_type("rtmp");
}

RtmpPublishClientSession::~RtmpPublishClientSession() { spdlog::debug("destroy RtmpPublishClientSession"); }

void RtmpPublishClientSession::on_socket_open(std::shared_ptr<SocketInterface> sock) { (void)sock; }

void RtmpPublishClientSession::on_socket_close(std::shared_ptr<SocketInterface> sock) { (void)sock; }

std::shared_ptr<RtmpMediaSink> RtmpPublishClientSession::get_rtmp_media_sink() { return rtmp_media_sink_; }

void RtmpPublishClientSession::service() {
    auto self(this->shared_from_this());
    // 根据url获取信息
    std::string protocol;
    std::string domain;
    uint16_t port;
    std::string path;
    std::unordered_map<std::string, std::string> params;
    if (!Utils::parse_url(url_, protocol, domain, port, path, params)) {  // todo:add log
        spdlog::error("parse url:{} failed", url_);
        return;
    }

    if (protocol == "rtmp") {
        conn_ = std::make_shared<TcpSocket>(this, worker_);
    } else if (protocol == "rtmps") {
        conn_ = std::make_shared<TlsSocket>(this, worker_);
    } else {
        return;
    }

    handshake_ = std::make_unique<RtmpHandshake>(conn_);
    chunk_protocol_ = std::make_unique<RtmpChunkProtocol>(conn_);

    boost::asio::co_spawn(
        conn_->get_worker()->get_io_context(),
        [this, self, domain, path, port]() -> boost::asio::awaitable<void> {
            spdlog::debug("start push rtmp, url:{}", url_);
            // 解析app名称
            std::vector<std::string> vs;
            boost::split(vs, path, boost::is_any_of("/"));
            if (vs.size() <= 2) {
                co_return;
            }
            set_session_info(domain, vs[1], vs[2]);
            // 获取域名ip
            auto ip = DnsService::get_instance().get_ip(domain);
            if (!ip) {
                spdlog::debug("could not find ip for:%s", domain.c_str());
                co_return;
            }
            auto &server_ip = ip.value();
            // 连接
            if (!co_await conn_->connect(server_ip, port)) {
                spdlog::debug("RtmpPublishClientSession connect to {}:{} failed", server_ip, port);
                co_return;
            }

            // 启动握手
            if (!co_await handshake_->do_client_handshake()) {
                spdlog::debug("RtmpPublishClientSession do_client_handshake failed");
                co_return;
            }
            spdlog::debug("RtmpPublishClientSession do_client_handshake done");
            // 启动发送协程
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
                        if (!(co_await chunk_protocol_->send_rtmp_messages(rtmp_msgs))) {
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
                [this, self](std::exception_ptr exp) {
                    (void)exp;
                    wg_.done();
                    close();
                    spdlog::debug("RtmpPublishClientSession send coroutine exited");
                });

            // 循环接收chunk层收到的rtmp消息
            wg_.add(1);
            boost::asio::co_spawn(
                conn_->get_worker()->get_io_context(),
                [this, self]() -> boost::asio::awaitable<void> {
                    int32_t ret = co_await chunk_protocol_->cycle_recv_rtmp_message(std::bind(
                        &RtmpPublishClientSession::on_recv_rtmp_message, this, std::placeholders::_1));
                    spdlog::debug("RtmpPublishClientSession cycle_recv_rtmp_message return, ret:{}", ret);
                },
                [this, self](std::exception_ptr exp) {
                    (void)exp;
                    wg_.done();
                    close();
                });
            start_alive_checker();

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

            RtmpReleaseStreamMessage release_stream_msg(transaction_id_++, stream_and_params);
            if (!co_await send_rtmp_message(release_stream_msg)) {
                spdlog::error("send create stream msg failed");
                co_return;
            }

            RtmpFCPublishMessage publish_msg(transaction_id_++, stream_and_params);
            if (!co_await send_rtmp_message(publish_msg)) {
                co_return;
            }

            RtmpCreateStreamMessage create_msg(transaction_id_++);
            if (!co_await send_rtmp_message(create_msg)) {
                co_return;
            }

            RtmpPublishMessage pub_msg(transaction_id_++, stream_and_params, "live");
            if (!co_await send_rtmp_message(pub_msg)) {
                co_return;
            }

            rtmp_media_sink_->on_rtmp_message(
                [this](const std::vector<std::shared_ptr<RtmpMessage>> &rtmp_msgs)
                    -> boost::asio::awaitable<bool> {
                    co_await rtmp_msgs_channel_.async_send(boost::system::error_code{}, rtmp_msgs, nullptr,
                                                           boost::asio::use_awaitable);
                    co_return true;
                });

            co_return;
        },
        boost::asio::detached);
}

void RtmpPublishClientSession::close() {
    // todo: how to record 404 error to log.
    if (closed_.test_and_set(std::memory_order_acquire)) {
        return;
    }

    auto self(this->shared_from_this());
    boost::asio::co_spawn(
        conn_->get_worker()->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            spdlog::debug("closing RtmpPublishClientSession...");
            check_closable_timer_.cancel();
            alive_timeout_timer_.cancel();
            if (conn_) {
                conn_->close();
            }

            if (rtmp_msgs_channel_.is_open()) {  // 结束发送协程
                rtmp_msgs_channel_.close();
            }

            co_await wg_.wait();

            if (rtmp_media_sink_) {
                auto s = SourceManager::get_instance().get_source(get_domain_name(), get_app_name(),
                                                                  get_stream_name());
                if (s) {
                    s->remove_media_sink(rtmp_media_sink_);
                }

                if (rtmp_media_sink_) {
                    rtmp_media_sink_->on_rtmp_message({});
                    rtmp_media_sink_->close();
                    rtmp_media_sink_ = nullptr;
                }
            }

            if (conn_) {
                conn_.reset();
            }
            spdlog::debug("close RtmpPublishClientSession done");
            co_return;
        },
        boost::asio::detached);
}

void RtmpPublishClientSession::set_url(const std::string &url) { url_ = url; }

boost::asio::awaitable<bool> RtmpPublishClientSession::send_acknowledge_msg_if_required() {
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

void RtmpPublishClientSession::start_alive_checker() {
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
                if (now_ms - last_active_time_ >= 5000) {  // 5秒没数据，超时
                    spdlog::warn("rtmp session:{} is not alive, last_active_time:{}", get_session_name(),
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

void RtmpPublishClientSession::update_active_timestamp() { last_active_time_ = Utils::get_current_ms(); }

boost::asio::awaitable<bool> RtmpPublishClientSession::on_recv_rtmp_message(
    std::shared_ptr<RtmpMessage> rtmp_msg) {
    update_active_timestamp();
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
        case RTMP_MESSAGE_TYPE_AMF0_COMMAND: {
            co_return co_await handle_amf0_command(rtmp_msg);
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
boost::asio::awaitable<bool> RtmpPublishClientSession::send_rtmp_message(
    const std::vector<std::shared_ptr<RtmpMessage>> &rtmp_msgs) {
    co_await rtmp_msgs_channel_.async_send(boost::system::error_code{}, rtmp_msgs, nullptr,
                                           boost::asio::use_awaitable);
    co_return true;
}

boost::asio::awaitable<bool> RtmpPublishClientSession::handle_amf0_command(
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
    } else if (name == "onBWDone") {
    }

    co_return true;
}

boost::asio::awaitable<bool> RtmpPublishClientSession::handle_amf0_status_command(
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

    if (code.value() != RTMP_STATUS_STREAM_PUBLISH_START) {
        co_return false;
    }

    auto source = rtmp_source_.lock();
    if (!source) {
        close();
        co_return false;
    }

    source->add_media_sink(rtmp_media_sink_);
    co_return true;
}

boost::asio::awaitable<bool> RtmpPublishClientSession::handle_amf0_result_command(
    std::shared_ptr<RtmpMessage> rtmp_msg) {
    (void)rtmp_msg;
    co_return true;
}

bool RtmpPublishClientSession::handle_acknowledgement(std::shared_ptr<RtmpMessage> rtmp_msg) {
    // todo
    // nothing to do
    (void)rtmp_msg;
    return true;
}

bool RtmpPublishClientSession::handle_user_control_msg(std::shared_ptr<RtmpMessage> rtmp_msg) {
    (void)rtmp_msg;
    // todo handle user control msg
    return true;
}