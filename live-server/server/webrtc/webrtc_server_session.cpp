#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/ip/udp.hpp>
#include <iostream>
#include <variant>

using namespace boost::asio::experimental::awaitable_operators;

#include "app/app.h"
#include "app/play_app.h"
#include "app/publish_app.h"
#include "base/thread/thread_worker.hpp"
#include "base/utils/utils.h"
#include "bridge/media_bridge.hpp"
#include "codec/h264/h264_codec.hpp"
#include "config/app_config.h"
#include "config/config.h"
#include "core/rtp_media_sink.hpp"
#include "core/rtp_media_source.hpp"
#include "core/source_manager.hpp"
#include "dtls/boringssl_session.h"
#include "dtls/dtls_cert.h"
#include "json/json.h"
#include "protocol/http/http_request.hpp"
#include "protocol/http/http_response.hpp"
#include "protocol/http/websocket/websocket_packet.hpp"
#include "protocol/rtp/rtcp/rtcp_fb_pli.h"
#include "protocol/rtp/rtcp/rtcp_rr.hpp"
#include "protocol/rtp/rtcp/rtcp_sr.h"
#include "protocol/rtp/rtp_h264_packet.h"
#include "protocol/rtp/rtp_packet.h"
#include "protocol/sdp/attribute/common/dir.hpp"
#include "server/session.hpp"
#include "server/stun/protocol/stun_binding_response_msg.hpp"
#include "server/stun/protocol/stun_mapped_address_attr.h"
#include "server/stun/protocol/stun_msg.h"
#include "spdlog/spdlog.h"
#include "webrtc_media_source.hpp"
#include "webrtc_server.hpp"
#include "webrtc_server_session.hpp"

using namespace mms;
WebRtcServerSession::WebRtcServerSession(ThreadWorker *worker)
    : StreamSession(worker),
      worker_(worker),
      ws_msgs_coroutine_exited_(worker->get_io_context()),
      udp_msgs_channel_(worker->get_io_context()),
      alive_timeout_timer_(worker->get_io_context()),
      play_sdp_timeout_timer_(worker->get_io_context()),
      send_rtp_pkts_channel_(worker->get_io_context()),
      send_rtcp_fb_pkts_channel_(worker->get_io_context()),
      send_rtcp_pkts_channel_(worker->get_io_context()),
      send_funcs_channel_(worker->get_io_context()),
      send_pli_timer_(worker->get_io_context()),
      wg_(worker) {
    send_buf_ = std::make_unique<uint8_t[]>(1024 * 1024);
    enc_buf_ = std::make_unique<uint8_t[]>(1024 * 1024);

    local_ice_ufrag_ = Utils::get_rand_str(4);
    local_ice_pwd_ = Utils::get_rand_str(24);
}

void WebRtcServerSession::start_alive_checker() {
    auto self(shared_from_this());
    wg_.add(1);
    boost::asio::co_spawn(
        worker_->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            boost::system::error_code ec;
            while (1) {
                alive_timeout_timer_.expires_after(std::chrono::seconds(5));
                co_await alive_timeout_timer_.async_wait(
                    boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                if (ec) {
                    co_return;
                }
                int64_t now_ms = Utils::get_current_ms();
                CORE_DEBUG("session:{} checking alive...", get_session_name());
                if (now_ms - last_active_time_ >= 50000) {  // 5秒没数据，超时
                    co_return;
                }
                CORE_DEBUG("session:{} is alive", get_session_name());
            }
            co_return;
        },
        [this, self](std::exception_ptr exp) {
            (void)exp;
            wg_.done();
            close();
        });
}

void WebRtcServerSession::update_active_timestamp() { last_active_time_ = Utils::get_current_ms(); }

boost::asio::awaitable<void> WebRtcServerSession::stop_alive_checker() {
    alive_timeout_timer_.cancel();
    co_return;
}

boost::asio::awaitable<void> WebRtcServerSession::async_process_udp_msg(
    UdpSocket *sock, std::unique_ptr<uint8_t[]> data, size_t len, boost::asio::ip::udp::endpoint &remote_ep) {
    co_await udp_msgs_channel_.async_send(boost::system::error_code{}, sock, std::move(data), len, remote_ep,
                                          boost::asio::use_awaitable);
    co_return;
}

WebRtcServerSession::~WebRtcServerSession() { CORE_DEBUG("destroy WebRtcServerSession"); }

std::string WebRtcServerSession::get_local_ip() const { return local_ip_; }

void WebRtcServerSession::set_local_ip(const std::string &v) { local_ip_ = v; }

uint16_t WebRtcServerSession::get_udp_port() const { return udp_port_; }

void WebRtcServerSession::set_udp_port(uint16_t v) { udp_port_ = v; }

void WebRtcServerSession::start_process_recv_udp_msg() {
    auto self(this->shared_from_this());
    dtls_boringssl_session_ = std::make_shared<DtlsBoringSSLSession>(worker_, *this);

    boost::asio::co_spawn(
        worker_->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            dtls_boringssl_session_->on_handshake_done(
                std::bind(&WebRtcServerSession::on_dtls_handshake_done, this, std::placeholders::_1,
                          std::placeholders::_2, std::placeholders::_3));
            auto ret =
                co_await dtls_boringssl_session_->do_handshake(DtlsBoringSSLSession::mode_server, dtls_cert_);
            if (0 != ret) {  // handshake failed
                co_return;
            }
            // dtls握手成功，启动rtcp数据发送
            start_rtcp_fb_sender();
            start_rtcp_sender();
            if (is_player_) {
                start_rtp_sender();
            } else if (is_publisher_) {
                start_pli_sender();
            }
            co_return;
        },
        boost::asio::detached);

    wg_.add(1);
    boost::asio::co_spawn(
        worker_->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            boost::system::error_code ec;
            while (1) {
                auto [sock, udp_msg, len, remote_ep] = co_await udp_msgs_channel_.async_receive(
                    boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                if (ec) {
                    break;
                }
                update_active_timestamp();
                UDP_MSG_TYPE msg_type = detect_msg_type(udp_msg.get(), len);
                if (UDP_MSG_DTLS == msg_type) {
                    if (!co_await process_dtls_packet(std::move(udp_msg), len, sock, remote_ep)) {
                        continue;
                    }
                } else if (UDP_MSG_RTP == msg_type) {
                    if (!co_await process_srtp_packet(std::move(udp_msg), len, sock, remote_ep)) {
                        continue;
                    }
                }
            }
            co_return;
        },
        [this, self](std::exception_ptr exp) {
            (void)exp;
            wg_.done();
            close();
        });
}

boost::asio::awaitable<void> WebRtcServerSession::stop_process_recv_udp_msg() {
    if (udp_msgs_channel_.is_open()) {
        udp_msgs_channel_.close();
    }

    if (dtls_boringssl_session_) {
        co_await dtls_boringssl_session_->close();
    }

    co_return;
}

void WebRtcServerSession::start_pli_sender() {
    auto self(this->shared_from_this());
    wg_.add(1);
    boost::asio::co_spawn(
        worker_->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            boost::system::error_code ec;
            while (1) {
                send_pli_timer_.expires_after(std::chrono::milliseconds(2000));  // 2s发一次
                co_await send_pli_timer_.async_wait(
                    boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                if (boost::asio::error::operation_aborted == ec) {
                    break;
                }

                std::shared_ptr<RtcpFbPli> pli_pkt = std::make_shared<RtcpFbPli>();
                pli_pkt->set_ssrc(video_ssrc_);
                pli_pkt->set_media_source_ssrc(video_ssrc_);
                co_await send_rtcp_fb_pkts_channel_.async_send(boost::system::error_code{}, pli_pkt,
                                                               boost::asio::use_awaitable);
            }
            co_return;
        },
        [this, self](std::exception_ptr exp) {
            (void)exp;
            wg_.done();
            close();
        });
}

boost::asio::awaitable<void> WebRtcServerSession::stop_pli_sender() {
    send_pli_timer_.cancel();
    co_return;
}

void WebRtcServerSession::start_rtp_sender() {
    auto self(this->shared_from_this());
    wg_.add(1);
    boost::asio::co_spawn(
        worker_->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            boost::system::error_code ec;
            spdlog::info("rtp sender running");
            while (1) {
                auto rtp_pkts = co_await send_rtp_pkts_channel_.async_receive(
                    boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                if (ec) {
                    break;
                }

                for (auto rtp_pkt : rtp_pkts) {
                    // if (rtp_pkt->get_pt() == video_pt_) {
                    //     spdlog::info("recv pt:{:x}, num:{}", rtp_pkt->get_pt(), rtp_pkt->get_seq_num());
                    // }

                    auto rtp_size = rtp_pkt->encode((uint8_t *)send_buf_.get(), 1024 * 1024);
                    size_t enc_buf_size = 1024 * 1024;
                    auto r = srtp_session_.protect_srtp((uint8_t *)send_buf_.get(), rtp_size,
                                                        (uint8_t *)enc_buf_.get(), enc_buf_size);
                    if (r < 0) {
                        spdlog::error("protect srtp failed, rtp_size:{}, ret:{}, seq:{}, pt:{:x}", rtp_size,
                                      r, rtp_pkt->get_seq_num(), rtp_pkt->get_pt());
                        co_return;
                    } else if (r == 0) {
                        continue;
                    }

                    if (!co_await send_rtp_socket_->send_to(enc_buf_.get(), enc_buf_size, peer_ep)) {
                        co_return;
                    }
                    update_active_timestamp();
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

boost::asio::awaitable<void> WebRtcServerSession::stop_rtp_sender() {
    if (send_rtp_pkts_channel_.is_open()) {
        send_rtp_pkts_channel_.close();
    }
    co_return;
}

void WebRtcServerSession::start_rtcp_fb_sender() {
    auto self(this->shared_from_this());
    wg_.add(1);
    boost::asio::co_spawn(
        worker_->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            boost::system::error_code ec;
            fb_buf_ = std::make_unique<uint8_t[]>(65536);
            enc_fb_buf_ = std::make_unique<uint8_t[]>(65536);
            while (1) {
                auto fb_pkt = co_await send_rtcp_fb_pkts_channel_.async_receive(
                    boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                if (ec) {
                    break;
                }

                if (fb_pkt) {
                    auto fb_size = fb_pkt->encode((uint8_t *)fb_buf_.get(), 65536);
                    size_t enc_fb_buf_size = 65536;

                    auto r = srtp_session_.protect_srtcp((uint8_t *)fb_buf_.get(), fb_size,
                                                         (uint8_t *)enc_fb_buf_.get(), enc_fb_buf_size);
                    if (r < 0) {
                        spdlog::error("protect srtcp failed");
                        co_return;
                    } else if (r == 0) {
                        continue;
                    }

                    if (!co_await send_rtp_socket_->send_to(enc_fb_buf_.get(), enc_fb_buf_size, peer_ep)) {
                        co_return;
                    }
                }
            }
            co_return;
        },
        [this, self](std::exception_ptr exp) {
            (void)exp;
            wg_.done();
            close();
        });
}

boost::asio::awaitable<void> WebRtcServerSession::stop_rtcp_fb_sender() {
    if (send_rtcp_fb_pkts_channel_.is_open()) {
        send_rtcp_fb_pkts_channel_.close();
    }
    co_return;
}

void WebRtcServerSession::start_rtcp_sender() {
    auto self(this->shared_from_this());
    wg_.add(1);
    boost::asio::co_spawn(
        worker_->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            boost::system::error_code ec;
            auto send_buf = std::make_unique<uint8_t[]>(65536);
            auto encoded_buf = std::make_unique<uint8_t[]>(65536);
            while (1) {
                auto rtcp_pkt = co_await send_rtcp_pkts_channel_.async_receive(
                    boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                if (ec) {
                    break;
                }

                if (rtcp_pkt) {
                    auto rtcp_size = rtcp_pkt->encode((uint8_t *)send_buf.get(), 65536);
                    size_t enc_rtcp_buf_size = 65536;
                    auto r = srtp_session_.protect_srtcp((uint8_t *)send_buf.get(), rtcp_size,
                                                         (uint8_t *)encoded_buf.get(), enc_rtcp_buf_size);
                    if (r < 0) {
                        spdlog::error("protect srtcp failed");
                        co_return;
                    } else if (r == 0) {
                        continue;
                    }

                    if (!co_await send_rtp_socket_->send_to(encoded_buf.get(), enc_rtcp_buf_size, peer_ep)) {
                        co_return;
                    }
                }
            }
            co_return;
        },
        [this, self](std::exception_ptr exp) {
            (void)exp;
            wg_.done();
            close();
        });
}

boost::asio::awaitable<void> WebRtcServerSession::stop_rtcp_sender() {
    if (send_rtcp_pkts_channel_.is_open()) {
        send_rtcp_pkts_channel_.close();
    }
    co_return;
}

boost::asio::awaitable<bool> WebRtcServerSession::process_whip_req(std::shared_ptr<HttpRequest> req,
                                                                   std::shared_ptr<HttpResponse> resp) {
    auto &query_params = req->get_query_params();
    for (auto &p : query_params) {
        set_param(p.first, p.second);
    }

    std::string domain = req->get_query_param("domain");
    if (domain.empty()) {
        domain = req->get_header("Host");
        auto pos = domain.find(":");
        if (pos != std::string::npos) {
            domain = domain.substr(0, pos);
        }
    }
    auto app_name = req->get_path_param("app");
    auto stream_name = req->get_path_param("stream");
    set_session_info(domain, app_name, stream_name);
    if (!find_and_set_app(domain_name_, app_name_, false)) {
        CORE_ERROR("could not find config for domain:{}, app:{}", domain_name_, app_name_);
        resp->add_header("Connection", "Close");
        co_await resp->write_header(403, "Forbidden");
        close();
        co_return false;
    }
    auto sdp = req->get_body();
    auto play_app = std::static_pointer_cast<PlayApp>(get_app());
    if (!play_app) {
        resp->add_header("Connection", "Close");
        co_await resp->write_header(403, "Forbidden");
        close();
        co_return false;
    }

    auto publish_app = play_app->get_publish_app();
    if (!publish_app) {
        resp->add_header("Connection", "Close");
        co_await resp->write_header(403, "Forbidden");
        close();
        co_return false;
    }

    auto self(std::static_pointer_cast<StreamSession>(shared_from_this()));
    is_publisher_ = true;
    webrtc_media_source_ =
        std::make_shared<WebRtcMediaSource>(get_worker(), std::weak_ptr<StreamSession>(self), publish_app);
    std::string answer_sdp = webrtc_media_source_->process_publish_sdp(sdp);
    if (answer_sdp.empty()) {
        CORE_ERROR("process publish sdp failed");
        co_return false;
    }

    if (!SourceManager::get_instance().add_source(get_domain_name(), get_app_name(), get_stream_name(),
                                                  webrtc_media_source_)) {
        CORE_ERROR("add webrtc source failed.");
        co_return false;
    }

    video_ssrc_ = webrtc_media_source_->get_video_ssrc();
    audio_ssrc_ = webrtc_media_source_->get_audio_ssrc();
    start_process_recv_udp_msg();

    resp->add_header("Content-Type", "application/sdp");
    if (!co_await resp->write_header(201, "Created")) {
        co_return false;
    }

    if (!co_await resp->write_data(answer_sdp)) {
        co_return false;
    }

    CORE_DEBUG("send publish answer sdp:{}", answer_sdp);
    co_return true;
}

boost::asio::awaitable<bool> WebRtcServerSession::process_whep_req(std::shared_ptr<HttpRequest> req,
                                                                   std::shared_ptr<HttpResponse> resp) {
    auto &query_params = req->get_query_params();
    for (auto &p : query_params) {
        set_param(p.first, p.second);
    }

    std::string domain = req->get_query_param("domain");
    if (domain.empty()) {
        domain = req->get_header("Host");
        auto pos = domain.find(":");
        if (pos != std::string::npos) {
            domain = domain.substr(0, pos);
        }
    }
    auto app_name = req->get_path_param("app");
    auto stream_name = req->get_path_param("stream");
    set_session_info(domain, app_name, stream_name);
    if (!find_and_set_app(domain_name_, app_name_, false)) {
        CORE_ERROR("could not find config for domain:{}, app:{}", domain_name_, app_name_);
        resp->add_header("Connection", "Close");
        co_await resp->write_header(403, "Forbidden");
        close();
        co_return false;
    }

    auto play_app = std::static_pointer_cast<PlayApp>(get_app());
    if (!play_app) {
        co_return false;
    }

    auto publish_app = play_app->get_publish_app();
    if (!publish_app) {
        co_return false;
    }

    auto self(std::static_pointer_cast<WebRtcServerSession>(shared_from_this()));
    // 1.本机查找
    auto source =
        SourceManager::get_instance().get_source(get_domain_name(), get_app_name(), get_stream_name());
    if (source) {
        spdlog::info("find source from local");
    }

    if (!source) {  // 2.本地配置查找外部回源
        source = co_await publish_app->find_media_source(self);
    }
    // 3.到media center中心查找
    // if (!source) {
    //     source = co_await MediaCenterManager::get_instance().find_and_create_pull_media_source(self);
    // }

    if (!source) {
        // 真找不到源流了，应该是没在播
        co_return false;
    }

    std::shared_ptr<MediaBridge> bridge;
    if (source->get_media_type() != "webrtc{rtp[es]}") {
        bridge = source->get_or_create_bridge(source->get_media_type() + "-webrtc{rtp[es]}", publish_app,
                                              stream_name_);
        if (!bridge) {
            co_return false;
        }
        source = bridge->get_media_source();
    } else {
        spdlog::info("source is webrtc{rtp[es]}");
    }

    is_player_ = true;
    rtp_media_sink_ = std::make_shared<RtpMediaSink>(get_worker());

    rtp_media_sink_->set_video_pkts_cb(
        [this](std::vector<std::shared_ptr<RtpPacket>> rtp_pkts) -> boost::asio::awaitable<bool> {
            co_await send_rtp_pkts_channel_.async_send(boost::system::error_code{}, rtp_pkts,
                                                       boost::asio::use_awaitable);
            co_return true;
        });

    rtp_media_sink_->set_audio_pkts_cb(
        [this](std::vector<std::shared_ptr<RtpPacket>> rtp_pkts) -> boost::asio::awaitable<bool> {
            co_await send_rtp_pkts_channel_.async_send(boost::system::error_code{}, rtp_pkts,
                                                       boost::asio::use_awaitable);
            co_return true;
        });

    start_process_recv_udp_msg();
    source->add_media_sink(rtp_media_sink_);
    // todo: 处理桥接的问题
    auto webrtc_source = std::static_pointer_cast<WebRtcMediaSource>(source);
    int32_t try_get_play_sdp_count = 0;
    std::shared_ptr<Sdp> play_sdp;
    while (1) {
        play_sdp = webrtc_source->get_play_offer_sdp();
        if (play_sdp) {
            break;
        }

        try_get_play_sdp_count++;
        if (try_get_play_sdp_count >= 400) {
            break;
        }

        boost::system::error_code ec;
        play_sdp_timeout_timer_.expires_after(std::chrono::milliseconds(10));
        co_await play_sdp_timeout_timer_.async_wait(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) {
            break;
        }
    }

    if (!play_sdp) {
        spdlog::info("could not get play sdp");
        resp->add_header("Connection", "Close");
        co_await resp->write_header(404, "Not Found");
        close();
        co_return false;
    }

    auto &medias = play_sdp->get_media_sdps();
    for (auto &media : medias) {
        media.set_ice_ufrag(IceUfrag(local_ice_ufrag_));
        media.set_ice_pwd(IcePwd(local_ice_pwd_));
    }

    CORE_DEBUG("send play offer sdp:{}", play_sdp->to_string());
    resp->add_header("Content-Type", "application/sdp");
    if (!co_await resp->write_header(201, "Created")) {
        co_return false;
    }

    if (!co_await resp->write_data(play_sdp->to_string())) {
        co_return false;
    }
    co_return true;
}

// boost::asio::awaitable<bool> WebRtcServerSession::process_play_answer(const Json::Value & root)
// {
//     if (!root.isMember("app") || !root["app"].isString())
//     {
//         co_return false;
//     }

//     if (!root.isMember("stream") || !root["stream"].isString())
//     {
//         co_return false;
//     }

//     const Json::Value &msg = root["message"];
//     if (!msg.isMember("type") || !msg["type"].isString())
//     {
//         spdlog::error("msg type is not string, close session");
//         co_return false;
//     }

//     const std::string &type = msg["type"].asString();
//     if ("answer" != type)
//     {
//         co_return false;
//     }

//     if (!msg.isMember("sdp") || !msg["sdp"].isString())
//     {
//         spdlog::error("no sdp info, close session");
//         co_return false;
//     }

//     if (!req_) {
//         co_return false;
//     }

//     std::string domain = req->get_header("Host");
//     const std::string &app = root["app"].asString();
//     const std::string &stream = root["stream"].asString();

//     auto ret = remote_sdp_.parse(msg["sdp"].asString());
//     if (0 != ret)
//     {
//         co_return false;
//     }

//     auto remote_ice_ufrag = remote_sdp_.get_ice_ufrag();
//     if (!remote_ice_ufrag)
//     {
//         co_return false;
//     }
//     remote_ice_ufrag_ = remote_ice_ufrag.value().getUfrag();

//     auto remote_ice_pwd = remote_sdp_.get_ice_pwd();
//     if (!remote_ice_pwd)
//     {
//         co_return false;
//     }
//     remote_ice_pwd_ = remote_ice_pwd.value().getPwd();

//     set_session_info(domain, app, stream);
//     auto source = SourceManager::get_instance().get_source(get_session_name());
//     CORE_DEBUG("get play answer:{}", root.toStyledString());
//     co_return true;
// }

boost::asio::awaitable<bool> WebRtcServerSession::process_stun_packet(
    std::shared_ptr<StunMsg> stun_msg, std::unique_ptr<uint8_t[]> data, size_t len, UdpSocket *sock,
    const boost::asio::ip::udp::endpoint &remote_ep) {
    const std::string &pwd = get_local_ice_pwd();
    if (!stun_msg->check_msg_integrity(data.get(), len, pwd)) {
        spdlog::error("check msg integrity failed");
        co_return false;
    }

    if (!stun_msg->check_finger_print(data.get(), len)) {
        spdlog::error("check finger print failed.");
        co_return false;
    }

    switch (stun_msg->type()) {
        case STUN_BINDING_REQUEST: {
            // 返回响应
            auto ret = co_await process_stun_binding_req(stun_msg, sock, remote_ep);
            co_return ret;
        }
    }
    co_return false;
}

boost::asio::awaitable<bool> WebRtcServerSession::process_stun_binding_req(
    std::shared_ptr<StunMsg> stun_msg, UdpSocket *sock, const boost::asio::ip::udp::endpoint &remote_ep) {
    StunBindingResponseMsg binding_resp(*stun_msg);

    auto mapped_addr_attr =
        std::make_unique<StunMappedAddressAttr>(remote_ep.address().to_v4().to_uint(), remote_ep.port());
    binding_resp.add_attr(std::move(mapped_addr_attr));

    // 校验完整性
    auto req_username_attr = stun_msg->get_username_attr();
    if (!req_username_attr) {
        co_return false;
    }

    const std::string &local_user_name = req_username_attr.value().get_local_user_name();
    if (local_user_name.empty()) {
        co_return false;
    }

    const std::string &remote_user_name = req_username_attr.value().get_remote_user_name();
    if (remote_user_name.empty()) {
        co_return false;
    }

    if (remote_user_name != remote_ice_ufrag_)  // 用户名与sdp中给的不一致
    {
        co_return false;
    }

    StunUsernameAttr resp_username_attr(local_user_name, remote_user_name);
    binding_resp.set_username_attr(resp_username_attr);
    auto size = binding_resp.size(true, true);
    std::unique_ptr<uint8_t[]> data = std::unique_ptr<uint8_t[]>(new uint8_t[size]);
    int32_t consumed = binding_resp.encode(data.get(), size, true, local_ice_pwd_, true);
    if (consumed < 0) {  // todo:add log
        co_return false;
    }

    if (!(co_await sock->send_to(data.get(), size, remote_ep))) {  // todo log error
        co_return false;
    }
    peer_ep = remote_ep;
    co_return true;
}

boost::asio::awaitable<bool> WebRtcServerSession::process_dtls_packet(
    std::unique_ptr<uint8_t[]> recv_data, size_t len, UdpSocket *sock,
    const boost::asio::ip::udp::endpoint &remote_ep) {
    co_return co_await dtls_boringssl_session_->process_dtls_packet(recv_data.get(), len, sock, remote_ep);
}

void WebRtcServerSession::set_dtls_cert(std::shared_ptr<DtlsCert> dtls_cert) { dtls_cert_ = dtls_cert; }

void WebRtcServerSession::on_dtls_handshake_done(SRTPProtectionProfile profile,
                                                 const std::string &srtp_recv_key,
                                                 const std::string &srtp_send_key) {
    if (!srtp_session_.init(profile, srtp_recv_key, srtp_send_key)) {
        spdlog::error("srtp session init failed");
    } else {
        spdlog::info("srtp session init succeed!!!");
    }
}

boost::asio::awaitable<bool> WebRtcServerSession::process_srtp_packet(
    std::unique_ptr<uint8_t[]> recv_data, size_t len, UdpSocket *sock,
    const boost::asio::ip::udp::endpoint &remote_ep) {
    ((void)sock);
    ((void)remote_ep);
    ((void)recv_data);
    ((void)len);
    ((void)sock);
    uint8_t *data = recv_data.get();
    int out_len = 0;
    if (RtpHeader::is_rtcp_packet(data, len)) {
        out_len = srtp_session_.unprotect_srtcp(data, len);
        if (out_len < 0) {
            co_return false;
        }

        auto pt = RtcpHeader::parse_pt(data, len);
        switch (pt) {
            case PT_RTCP_SR: {
                RtcpSr rtcp_sr;
                int32_t consumed = rtcp_sr.decode(data, len);
                if (consumed <= 0) {
                    co_return false;
                }

                auto ntp = Utils::get_ntp_time();
                if (rtcp_sr.header_.sender_ssrc == video_ssrc_) {
                    recv_sr_packets_[video_ssrc_] = rtcp_sr;
                    std::shared_ptr<RtcpRR> rtcp_rr = std::make_shared<RtcpRR>();
                    rtcp_rr->set_ssrc(video_ssrc_);
                    ReceptionReportBlock report;
                    report.ssrc = video_ssrc_;
                    report.fraction_lost = 0;
                    report.cumulative_number_of_packets_lost = 0;
                    report.extended_highest_sequence_number_received =
                        video_extended_highest_sequence_number_received_;
                    report.interarrival_jitter = video_interarrival_jitter_;
                    report.last_SR = video_last_sr_ntp_ >> 16;
                    report.delay_since_last_SR = (ntp >> 16) - (video_last_sr_sys_ntp_ >> 16);
                    rtcp_rr->add_reception_report_block(report);
                    video_last_sr_ntp_ =
                        ((uint64_t)rtcp_sr.ntp_timestamp_sec_ << 32) | (rtcp_sr.ntp_timestamp_psec_);
                    video_last_sr_sys_ntp_ = ntp;
                    co_await send_rtcp_pkts_channel_.async_send(boost::system::error_code{}, rtcp_rr,
                                                                boost::asio::use_awaitable);
                } else if (rtcp_sr.header_.sender_ssrc == audio_ssrc_) {
                    recv_sr_packets_[audio_ssrc_] = rtcp_sr;
                    std::shared_ptr<RtcpRR> rtcp_rr = std::make_shared<RtcpRR>();
                    rtcp_rr->set_ssrc(audio_ssrc_);
                    ReceptionReportBlock report;
                    report.ssrc = audio_ssrc_;
                    report.fraction_lost = 0;
                    report.cumulative_number_of_packets_lost = 0;
                    report.extended_highest_sequence_number_received =
                        audio_extended_highest_sequence_number_received_;
                    report.interarrival_jitter = audio_interarrival_jitter_;
                    report.last_SR = audio_last_sr_ntp_ >> 16;
                    report.delay_since_last_SR = (ntp >> 16) - (audio_last_sr_sys_ntp_ >> 16);
                    rtcp_rr->add_reception_report_block(report);
                    audio_last_sr_ntp_ =
                        ((uint64_t)rtcp_sr.ntp_timestamp_sec_ << 32) | (rtcp_sr.ntp_timestamp_psec_);
                    audio_last_sr_sys_ntp_ = ntp;
                    co_await send_rtcp_pkts_channel_.async_send(boost::system::error_code{}, rtcp_rr,
                                                                boost::asio::use_awaitable);
                    // spdlog::info("audio rtp time:{}", rtcp_sr.rtp_time_/48000);
                }

                break;
            }
            default: {
                break;
            }
        }
    } else if (RtpHeader::is_rtp(data, len)) {
        out_len = srtp_session_.unprotect_srtp(data, len);
        if (out_len < 0) {
            co_return false;
        }

        auto ssrc = RtpHeader::parse_ssrc(data, out_len);  // todo 改成用ssrc来区分
        if (ssrc == webrtc_media_source_->get_audio_ssrc()) {
            std::shared_ptr<RtpPacket> rtp_pkt = std::make_shared<RtpPacket>();
            int32_t consumed = rtp_pkt->decode_and_store(data, out_len);
            if (consumed < 0) {
                co_return false;
            }

            uint64_t now_ms_in_timestamp_units = Utils::get_current_ms() * 48;
            if (audio_recv_in_timestamp_units_ != 0) {
                int32_t sj = rtp_pkt->get_timestamp();
                int32_t si = audio_recv_rtp_ts_;
                auto rj = now_ms_in_timestamp_units;
                auto ri = audio_recv_in_timestamp_units_;
                int32_t di_j = (rj - ri) - (sj - si);
                int32_t jitter_i = audio_interarrival_jitter_ + (abs(di_j) - audio_interarrival_jitter_) / 16;
                audio_interarrival_jitter_ = jitter_i;
            }
            audio_recv_in_timestamp_units_ = now_ms_in_timestamp_units;
            audio_recv_rtp_ts_ = rtp_pkt->get_timestamp();
            audio_extended_highest_sequence_number_received_ =
                rtp_pkt->get_seq_num();  // todo add 1 to high byte
            std::vector<std::shared_ptr<RtpPacket>> pkts;
            pkts.push_back(rtp_pkt);
            co_return co_await webrtc_media_source_->on_audio_packets(pkts);
        } else if (ssrc == webrtc_media_source_->get_video_ssrc()) {
            std::shared_ptr<RtpPacket> rtp_pkt = std::make_shared<RtpPacket>();
            int32_t consumed = rtp_pkt->decode_and_store(data, out_len);
            if (consumed < 0) {
                co_return false;
            }

            if (first_rtp_video_ts_ == 0) {
                first_rtp_video_ts_ = rtp_pkt->get_timestamp();
            }

            // RtpMediaSource::on_video_packet(rtp_pkt);
            auto h264_nalu = rtp_h264_depacketizer_.on_packet(rtp_pkt);
            if (h264_nalu) {
                uint32_t this_timestamp = h264_nalu->get_timestamp();
                bool key = find_key_frame((this_timestamp - first_rtp_video_ts_) / 90,
                                          h264_nalu);  // todo 除90这个要根据sdp来计算，目前固定
                if (key) {
                    CORE_DEBUG("WebRtcServerSession find key frame");
                }
            }

            uint64_t now_ms_in_timestamp_units = Utils::get_current_ms() * 90;
            if (video_recv_in_timestamp_units_ != 0) {
                int32_t sj = rtp_pkt->get_timestamp();
                int32_t si = video_recv_rtp_ts_;
                auto rj = now_ms_in_timestamp_units;
                auto ri = video_recv_in_timestamp_units_;
                int32_t di_j = (rj - ri) - (sj - si);
                int32_t jitter_i = video_interarrival_jitter_ + (abs(di_j) - video_interarrival_jitter_) / 16;
                video_interarrival_jitter_ = jitter_i;
            }
            video_recv_in_timestamp_units_ = now_ms_in_timestamp_units;
            video_recv_rtp_ts_ = rtp_pkt->get_timestamp();
            video_extended_highest_sequence_number_received_ =
                rtp_pkt->get_seq_num();  // todo add 1 to high byte
            std::vector<std::shared_ptr<RtpPacket>> pkts;
            pkts.push_back(rtp_pkt);
            co_return co_await webrtc_media_source_->on_video_packets(pkts);
        }
    }
    co_return false;
}

bool WebRtcServerSession::find_key_frame(uint32_t timestamp, std::shared_ptr<RtpH264NALU> &nalu) {
    (void)timestamp;
    bool is_key = false;
    auto &pkts = nalu->get_rtp_pkts();
    for (auto it = pkts.begin(); it != pkts.end(); it++) {
        auto pkt = it->second;
        // const uint8_t *pkt_buf = (const uint8_t*)pkt->get_payload().data();

        H264RtpPktInfo pkt_info;
        pkt_info.parse(pkt->get_payload().data(), pkt->get_payload().size());

        if (pkt_info.is_stap_a()) {
            std::string_view payload = pkt->get_payload();
            const char *data = payload.data() + 1;
            size_t pos = 1;

            while (pos < payload.size()) {
                uint16_t nalu_size = ntohs(*(uint16_t *)data);
                uint8_t nalu_type = *(data + 2) & 0x1F;
                if (nalu_type == H264NaluTypeIDR) {
                    is_key = true;
                }

                pos += 2 + nalu_size;
                data += 2 + nalu_size;
            }
        } else if (pkt_info.is_single_nalu()) {
            if (pkt_info.get_nalu_type() == H264NaluTypeIDR) {
                is_key = true;
            }
        } else if (pkt_info.get_type() == H264_RTP_PAYLOAD_FU_A) {
            if (pkt_info.is_start_fu()) {
                do {
                    if (pkt_info.is_start_fu()) {
                        if (pkt_info.get_nalu_type() == H264NaluTypeIDR) {
                            is_key = true;
                        }
                    }

                    if (pkt_info.is_end_fu()) {
                        break;
                    }
                    it++;

                    if (it != pkts.end()) {
                        pkt = it->second;
                        pkt_info.parse(pkt->get_payload().data(), pkt->get_payload().size());
                    }
                } while (pkt_info.get_type() == H264_RTP_PAYLOAD_FU_A && it != pkts.end());
            }
        }
    }
    return is_key;
}

void WebRtcServerSession::service() {
    start_alive_checker();
    return;
}

void WebRtcServerSession::close() {
    if (closed_.test_and_set(std::memory_order_acquire)) {
        return;
    }

    auto self(this->shared_from_this());
    boost::asio::co_spawn(
        get_worker()->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            CORE_DEBUG("closing webrtc sever session, step:stop_process_recv_udp_msg");
            co_await stop_process_recv_udp_msg();
            CORE_DEBUG("closing webrtc sever session, step:stop_pli_sender");
            co_await stop_pli_sender();
            CORE_DEBUG("closing webrtc sever session, step:stop_alive_checker");
            co_await stop_alive_checker();
            CORE_DEBUG("closing webrtc sever session, step:stop_rtp_sender");
            co_await stop_rtp_sender();
            CORE_DEBUG("closing webrtc sever session, step:stop_rtcp_fb_sender");
            co_await stop_rtcp_fb_sender();
            CORE_DEBUG("closing webrtc sever session, step:stop_rtcp_sender");
            co_await stop_rtcp_sender();
            co_await wg_.wait();
            boost::system::error_code ec;
            play_sdp_timeout_timer_.cancel();

            if (rtp_media_sink_) {
                auto source = SourceManager::get_instance().get_source(get_domain_name(), get_app_name(),
                                                                       get_stream_name());
                if (source) {
                    source->remove_media_sink(rtp_media_sink_);
                    rtp_media_sink_->close();
                    rtp_media_sink_.reset();
                }
            }

            if (webrtc_media_source_) {
                SourceManager::get_instance().remove_source(get_domain_name(), get_app_name(),
                                                            get_stream_name());
                webrtc_media_source_->close();
                webrtc_media_source_.reset();
            }

            if (close_handler_) {
                close_handler_->on_webrtc_session_close(
                    std::static_pointer_cast<WebRtcServerSession>(shared_from_this()));
            }

            CORE_DEBUG("close webrtc sever session done");
            co_return;
        },
        boost::asio::detached);
}

boost::asio::awaitable<bool> WebRtcServerSession::send_udp_msg(const uint8_t *data, size_t len) {
    co_return co_await send_rtp_socket_->send_to((uint8_t *)data, len, peer_ep);
}
