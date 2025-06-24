#include "rtsp_server_session.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>

#include "app/play_app.h"
#include "app/publish_app.h"
#include "base/network/socket_interface.hpp"
#include "base/utils/utils.h"
#include "bridge/media_bridge.hpp"
#include "config/app_config.h"
#include "config/config.h"
#include "core/rtsp_media_sink.hpp"
#include "core/rtsp_media_source.hpp"
#include "core/source_manager.hpp"
#include "log/log.h"
#include "protocol/rtsp/rtsp_request.hpp"
#include "protocol/rtsp/rtsp_response.hpp"
#include "protocol/sdp/sdp.hpp"
#include "rtp_over_tcp_pkt.hpp"
#include "server/session.hpp"

using namespace mms;
RtspServerSession::RtspServerSession(std::shared_ptr<SocketInterface> sock)
    : StreamSession(sock->get_worker()),
      sock_(sock),
      send_funcs_channel_(sock->get_worker()->get_io_context(), 2048),
      play_sdp_timeout_timer_(sock->get_worker()->get_io_context()),
      wg_(worker_) {
    set_session_type("rtsp");
    recv_buf_ = std::make_unique<char[]>(RTSP_MAX_BUF);
    send_buf_ = std::make_unique<char[]>(RTSP_MAX_BUF);
    session_id_ = Utils::get_rand64();
}

RtspServerSession::~RtspServerSession() { CORE_DEBUG("destroy RtspServerSession:{}", get_session_name()); }

void RtspServerSession::start_recv_coroutine() {
    auto self(shared_from_this());
    wg_.add(1);
    boost::asio::co_spawn(
        sock_->get_worker()->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            int32_t consumed = 0;
            while (1) {
                if (RTSP_MAX_BUF - recv_buf_size_ <= 0) {
                    break;
                }

                auto recv_size = co_await sock_->recv_some((uint8_t *)recv_buf_.get() + recv_buf_size_,
                                                           RTSP_MAX_BUF - recv_buf_size_);
                if (recv_size < 0) {  // response error
                    break;
                }

                recv_buf_size_ += recv_size;

                do {
                    consumed = 0;
                    std::string_view buf(recv_buf_.get(), recv_buf_size_);
                    if (buf.starts_with("RTSP")) {  // response
                        if (!rtsp_response_) {
                            rtsp_response_ = std::make_shared<RtspResponse>();
                        }
                        consumed = rtsp_response_->parse(buf);
                        if (consumed <= 0) {
                            break;
                        }
                        recv_buf_size_ -= consumed;
                        memmove(recv_buf_.get(), recv_buf_.get() + consumed, recv_buf_size_);
                        std::shared_ptr<RtspResponse> resp = std::move(rtsp_response_);
                        co_await on_rtsp_response(resp);
                    } else if (buf.starts_with("$")) {  // rtp or rtcp
                        if (!is_tcp_transport_) {
                            std::shared_ptr<RtspResponse> resp = std::make_shared<RtspResponse>();
                            resp->add_header("Connection", "Close");
                            resp->set_status(RTSP_STATUS_CODE_NOT_ACCEPTABLE, "Not Acceptable");
                            std::string resp_data = resp->to_string();
                            auto ret =
                                co_await sock_->send((const uint8_t *)resp_data.data(), resp_data.size());
                            if (!ret) {  // todo: add log
                                co_return;
                            }
                            co_return;
                        }

                        RtpOverTcpPktHeader rtp_over_tcp_pkt_header;
                        auto consumed1 =
                            rtp_over_tcp_pkt_header.decode((uint8_t *)recv_buf_.get(), recv_buf_size_);
                        if (consumed1 <= 0) {
                            break;
                        }

                        if (recv_buf_size_ - consumed1 < rtp_over_tcp_pkt_header.get_pkt_len()) {
                            break;
                        }

                        auto pt = RtpHeader::parse_pt((uint8_t *)recv_buf_.get() + consumed1,
                                                      recv_buf_size_ - consumed1);  // todo 改成用ssrc来区分
                        int32_t consumed2 = 0;
                        if (rtp_over_tcp_pkt_header.get_channel() % 2 == 0) {
                            if (pt == rtsp_media_source_->get_video_pt()) {
                                std::shared_ptr<RtpPacket> video_pkt = std::make_shared<RtpPacket>();
                                consumed2 =
                                    video_pkt->decode_and_store((uint8_t *)recv_buf_.get() + consumed1,
                                                                rtp_over_tcp_pkt_header.get_pkt_len());
                                if (consumed2 < 0) {
                                    co_return;
                                }
                                std::vector<std::shared_ptr<RtpPacket>> pkts = {video_pkt};
                                co_await rtsp_media_source_->on_video_packets(pkts);
                            } else if (pt == rtsp_media_source_->get_audio_pt()) {
                                std::shared_ptr<RtpPacket> audio_pkt = std::make_shared<RtpPacket>();
                                consumed2 =
                                    audio_pkt->decode_and_store((uint8_t *)recv_buf_.get() + consumed1,
                                                                rtp_over_tcp_pkt_header.get_pkt_len());
                                if (consumed2 < 0) {
                                    co_return;
                                }

                                std::vector<std::shared_ptr<RtpPacket>> pkts = {audio_pkt};
                                co_await rtsp_media_source_->on_audio_packets(pkts);
                            } else {
                            }
                        }

                        consumed = consumed1 + rtp_over_tcp_pkt_header.get_pkt_len();
                        recv_buf_size_ -= consumed;
                        memmove(recv_buf_.get(), recv_buf_.get() + consumed, recv_buf_size_);
                    } else {  // request
                        if (!rtsp_request_) {
                            rtsp_request_ = std::make_shared<RtspRequest>();
                        } else {
                            rtsp_request_->clear();
                        }

                        consumed = rtsp_request_->parse(buf);
                        if (consumed <= 0) {  // error
                            break;
                        }
                        recv_buf_size_ -= consumed;
                        memmove(recv_buf_.get(), recv_buf_.get() + consumed, recv_buf_size_);
                        std::shared_ptr<RtspRequest> req = std::move(rtsp_request_);
                        if (!co_await on_rtsp_request(req)) {
                            co_return;
                        }
                    }
                } while (recv_buf_size_ > 0 && consumed > 0);
                // 出错，response 4xx错误
                if (consumed < 0) {
                    boost::system::error_code ec;
                    std::shared_ptr<RtspResponse> resp = std::make_shared<RtspResponse>();
                    resp->add_header("Connection", "Close");
                    resp->set_status(RTSP_STATUS_CODE_NOT_ACCEPTABLE, "Not Acceptable");
                    co_await send_funcs_channel_.async_send(
                        boost::system::error_code{},
                        std::bind(&RtspServerSession::send_rtsp_resp, this, resp),
                        boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                    if (ec) {
                        co_return;
                    }
                }
            }

            co_return;
        },
        [this, self](std::exception_ptr exp) {
            (void)exp;
            stop();
            wg_.done();
        });
}

void RtspServerSession::start() {
    auto self(std::static_pointer_cast<RtspServerSession>(shared_from_this()));
    // todo:consider to wrap the conn as a bufio, and move parser to RtspRequest class.
    // 启动接收协程
    start_recv_coroutine();
    // 启动发送协程
    wg_.add(1);
    boost::asio::co_spawn(
        get_worker()->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            boost::system::error_code ec;
            while (1) {
                auto send_func = co_await send_funcs_channel_.async_receive(
                    boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                if (ec) {
                    break;
                }

                bool ret = co_await send_func();
                if (!ret) {
                    co_return;
                }
            }
            co_return;
        },
        [this, self](std::exception_ptr exp) {
            (void)exp;
            wg_.done();
            stop();
        });
}

boost::asio::awaitable<bool> RtspServerSession::send_rtsp_resp(std::shared_ptr<RtspResponse> rtsp_resp) {
    std::string resp_data = rtsp_resp->to_string();
    if (!co_await sock_->send((const uint8_t *)resp_data.data(), resp_data.size())) {
        co_return false;
    }
    co_return true;
}

boost::asio::awaitable<bool> RtspServerSession::send_rtp_over_tcp_pkts(
    std::vector<std::shared_ptr<RtpPacket>> rtp_pkts) {
    if (!is_tcp_transport_ || !rtsp_media_sink_) {
        co_return false;
    }

    for (auto rtp_pkt : rtp_pkts) {
        uint8_t channel;
        if (rtp_pkt->get_pt() == rtsp_media_sink_->get_video_pt()) {
            channel = 0;
        } else if (rtp_pkt->get_pt() == rtsp_media_sink_->get_audio_pt()) {
            channel = 2;
        }
        uint8_t header[20];
        RtpOverTcpPktHeader rtp_over_tcp_pkt_header;
        rtp_over_tcp_pkt_header.set_channel(channel);

        auto rtp_size = rtp_pkt->encode((uint8_t *)send_buf_.get(), RTSP_MAX_BUF);
        rtp_over_tcp_pkt_header.set_pkt_len(rtp_size);
        auto header_size = rtp_over_tcp_pkt_header.encode((uint8_t *)header, 20);
        if (!co_await sock_->send(header, header_size)) {
            co_return false;
        }

        if (!co_await sock_->send((uint8_t *)send_buf_.get(), rtp_size)) {
            co_return false;
        }
    }
    co_return true;
}

void RtspServerSession::stop() {
    if (closed_.test_and_set()) {
        return;
    }
    auto self(this->shared_from_this());
    boost::asio::co_spawn(
        get_worker()->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            CORE_DEBUG("closing rtsp sever session:{}", get_session_name());
            boost::system::error_code ec;
            if (sock_) {
                sock_->close();
            }

            send_funcs_channel_.close();
            co_await wg_.wait();

            if (rtsp_media_source_) {
                rtsp_media_source_->set_session(nullptr);
                auto publish_app = rtsp_media_source_->get_app();
                start_delayed_source_check_and_delete(publish_app->get_conf()->get_stream_resume_timeout(),
                                                      rtsp_media_source_);
                co_await publish_app->on_unpublish(
                    std::static_pointer_cast<StreamSession>(shared_from_this()));
            }

            if (rtsp_media_sink_) {
                auto source = rtsp_media_sink_->get_source();
                if (source) {
                    source->remove_media_sink(rtsp_media_sink_);
                }

                rtsp_media_sink_->on_close({});
                rtsp_media_sink_->close();
                rtsp_media_sink_.reset();
            }
            sock_.reset();
            CORE_DEBUG("close rtsp sever session:{} done", get_session_name());
            co_return;
        },
        boost::asio::detached);
}

boost::asio::awaitable<bool> RtspServerSession::on_rtsp_request(std::shared_ptr<RtspRequest> req) {
    boost::system::error_code ec;
    const std::string &method = req->get_method();
    if (method == RTSP_METHOD_OPTIONS) {
        co_return co_await process_options_req(req);
    } else if (method == RTSP_METHOD_SETUP) {
        co_return co_await process_setup_req(req);
    } else if (method == RTSP_METHOD_DESCRIBE) {
        co_return co_await process_describe_req(req);
    } else if (method == RTSP_METHOD_ANNOUNCE) {
        co_return co_await process_announce_req(req);
    } else if (method == RTSP_METHOD_RECORD) {
        co_return co_await process_record_req(req);
    } else if (method == RTSP_METHOD_PLAY) {
        co_return co_await process_play_req(req);
    } else if (method == RTSP_METHOD_TEARDOWN) {
        co_return co_await process_teardown_req(req);
    }

    std::shared_ptr<RtspResponse> resp = std::make_shared<RtspResponse>();
    resp->add_header("Connection", "close");
    resp->add_header("Server", "mms");
    resp->set_status(RTSP_STATUS_CODE_NOT_IMPLEMENTED, "Not Implemented");
    co_await send_funcs_channel_.async_send(boost::system::error_code{},
                                            std::bind(&RtspServerSession::send_rtsp_resp, this, resp),
                                            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    co_return true;
}

boost::asio::awaitable<bool> RtspServerSession::on_rtsp_response(std::shared_ptr<RtspResponse> req) {
    (void)req;
    co_return true;
}

boost::asio::awaitable<bool> RtspServerSession::process_options_req(std::shared_ptr<RtspRequest> req) {
    boost::system::error_code ec;
    std::shared_ptr<RtspResponse> resp = std::make_shared<RtspResponse>();
    resp->add_header("CSeq", req->get_header("CSeq"));
    resp->add_header(
        "Public", "OPTIONS, ANNOUNCE, DESCRIBE, SETUP, TEARDOWN, PLAY, RECORD, REDIRECT");  //, GET_PARAMETER,
                                                                                            //SET_PARAMETER
    resp->add_header("Server", "mms");
    resp->add_header("Session", std::to_string(session_id_) + ";timeout=60");
    co_await send_funcs_channel_.async_send(boost::system::error_code{},
                                            std::bind(&RtspServerSession::send_rtsp_resp, this, resp),
                                            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    co_return true;
}

boost::asio::awaitable<bool> RtspServerSession::process_announce_req(std::shared_ptr<RtspRequest> req) {
    boost::system::error_code ec;
    const std::string &uri = req->get_uri();
    std::string_view uri_view = uri;
    std::vector<std::string_view> vs;
    boost::split(vs, uri_view, boost::is_any_of("/"));  //  RTSP://DOMAIN/app/stream
    if (vs.size() != 5) {
        co_return false;
    }
    // parse domain
    std::string domain = req->get_query_param("domain");
    if (domain.empty()) {
        domain = req->get_header("Host");
        if (!domain.empty()) {
            auto pos = domain.find(":");
            if (pos != std::string::npos) {
                domain = domain.substr(0, pos);
            }
        }
    }

    if (domain.empty()) {
        auto pos = vs[2].find(":");
        if (pos != std::string_view::npos) {
            domain = vs[2].substr(0, pos);
        } else {
            domain = vs[2];
        }
    }

    set_session_info(domain, vs[3], vs[4]);
    // find app
    std::shared_ptr<RtspResponse> resp = std::make_shared<RtspResponse>();
    if (!find_and_set_app(domain, app_name_, true)) {
        CORE_ERROR("could not find conf for domain:{}, app:{}", domain, app_name_);
        resp->add_header("CSeq", req->get_header("CSeq"));
        resp->add_header("Connection", "Close");
        resp->add_header("Server", "mms");
        resp->set_status(RTSP_STATUS_CODE_FORBIDDEN, "Forbidden");
        co_await send_funcs_channel_.async_send(boost::system::error_code{},
                                                std::bind(&RtspServerSession::send_rtsp_resp, this, resp),
                                                boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        co_return true;
    }
    auto self(std::static_pointer_cast<StreamSession>(shared_from_this()));
    // publish auth check
    auto publish_app = std::static_pointer_cast<PublishApp>(app_);
    auto media_source =
        SourceManager::get_instance().get_source(get_domain_name(), get_app_name(), get_stream_name());
    if (!media_source) {
        media_source =
            std::make_shared<RtspMediaSource>(get_worker(), std::weak_ptr<StreamSession>(self), publish_app);
    }

    if (media_source->get_media_type() != "rtsp{rtp[es]}") {
        CORE_ERROR("source:{} is already exist and type is:{}", get_session_name(),
                   media_source->get_media_type());
        co_return true;
    }

    rtsp_media_source_ = std::static_pointer_cast<RtspMediaSource>(media_source);
    rtsp_media_source_->set_source_info(get_domain_name(), get_app_name(), get_stream_name());
    rtsp_media_source_->set_session(self);

    auto err = co_await publish_app->on_publish(self);
    if (err.code != 0) {
        CORE_ERROR("on publish http callback failed, session:{}", get_session_name());
        resp->add_header("CSeq", req->get_header("CSeq"));
        resp->add_header("Connection", "Close");
        resp->add_header("Server", "mms");
        resp->set_status(RTSP_STATUS_CODE_FORBIDDEN, "Forbidden");
        co_await send_funcs_channel_.async_send(boost::system::error_code{},
                                                std::bind(&RtspServerSession::send_rtsp_resp, this, resp),
                                                boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        co_return true;
    }
    // parse and check sdp
    CORE_DEBUG("session:{} rtsp announce sdp:{}", get_session_name(), req->get_body());
    auto ret = rtsp_media_source_->process_announce_sdp(req->get_body());
    if (!ret) {
        resp->add_header("CSeq", req->get_header("CSeq"));
        resp->add_header("Connection", "Close");
        resp->add_header("Server", "mms");
        resp->set_status(RTSP_STATUS_CODE_PARAMETER_NOT_UNDERSTOOD, "Parameter Not Understood");
        co_await send_funcs_channel_.async_send(boost::system::error_code{},
                                                std::bind(&RtspServerSession::send_rtsp_resp, this, resp),
                                                boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        co_return true;
    }

    is_publisher_ = true;
    resp->add_header("CSeq", req->get_header("CSeq"));
    resp->add_header("Server", "mms");
    resp->add_header("Session", std::to_string(session_id_) + ";timeout=60");
    resp->set_status(RTSP_STATUS_CODE_OK, "OK");
    co_await send_funcs_channel_.async_send(boost::system::error_code{},
                                            std::bind(&RtspServerSession::send_rtsp_resp, this, resp),
                                            boost::asio::redirect_error(boost::asio::use_awaitable, ec));

    ret = SourceManager::get_instance().add_source(get_domain_name(), get_app_name(), get_stream_name(),
                                                   rtsp_media_source_);
    if (!ret) {
        rtsp_media_source_->close();
        co_return false;
    }
    rtsp_media_source_->set_status(E_SOURCE_STATUS_OK);
    publish_app->on_create_source(get_domain_name(), get_app_name(), get_stream_name(), rtsp_media_source_);
    co_return true;
}

boost::asio::awaitable<bool> RtspServerSession::process_describe_req(std::shared_ptr<RtspRequest> req) {
    boost::system::error_code ec;
    std::shared_ptr<RtspResponse> resp = std::make_shared<RtspResponse>();
    resp->add_header("CSeq", req->get_header("CSeq"));
    resp->add_header("Server", "mms");
    resp->add_header("Session", std::to_string(session_id_) + ";timeout=60");

    const std::string &uri = req->get_uri();
    std::string_view uri_view = uri;
    std::vector<std::string_view> vs;
    boost::split(vs, uri_view, boost::is_any_of("/"));  //  RTSP://domain/app/stream
    if (vs.size() != 5) {
        co_return false;
    }
    // parse domain
    std::string domain = req->get_query_param("domain");
    if (domain.empty()) {
        domain = req->get_header("Host");
        if (!domain.empty()) {
            auto pos = domain.find(":");
            if (pos != std::string::npos) {
                domain = domain.substr(0, pos);
            }
        }
    }

    if (domain.empty()) {
        auto pos = vs[2].find(":");
        if (pos != std::string_view::npos) {
            domain = vs[2].substr(0, pos);
        } else {
            domain = vs[2];
        }
    }

    set_session_info(domain, vs[3], vs[4]);
    if (!find_and_set_app(domain_name_, app_name_, false)) {
        CORE_ERROR("could not find conf for session:{}", get_session_name());
        resp->add_header("Connection", "close");
        resp->set_status(403, "Forbidden");
        co_await send_funcs_channel_.async_send(boost::system::error_code{},
                                                std::bind(&RtspServerSession::send_rtsp_resp, this, resp),
                                                boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        co_return false;
    }

    auto play_app = std::static_pointer_cast<PlayApp>(get_app());
    auto self(std::static_pointer_cast<RtspServerSession>(shared_from_this()));
    auto err = co_await play_app->on_play(self);
    if (err.code != 0) {
        CORE_ERROR("play auth check failed, reason:{}", err.msg);
        resp->add_header("Connection", "Close");
        resp->set_status(403, "Forbidden");
        co_await send_funcs_channel_.async_send(boost::system::error_code{},
                                                std::bind(&RtspServerSession::send_rtsp_resp, this, resp),
                                                boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        co_return false;
    }

    auto publish_app = play_app->get_publish_app();
    if (!publish_app) {
        resp->add_header("Connection", "Close");
        resp->set_status(403, "Forbidden");
        co_await send_funcs_channel_.async_send(boost::system::error_code{},
                                                std::bind(&RtspServerSession::send_rtsp_resp, this, resp),
                                                boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        co_return false;
    }

    auto source_name = publish_app->get_domain_name() + "/" + app_name_ + "/" + stream_name_;
    // todo: how to record 404 error to log.
    auto source =
        SourceManager::get_instance().get_source(get_domain_name(), get_app_name(), get_stream_name());
    if (!source) {  // 2.本地配置查找外部回源
        source = co_await publish_app->find_media_source(self);
    }

    // 3.到media center中心查找
    // if (!source) {
    //     source = co_await MediaCenterManager::get_instance().find_and_create_pull_media_source(self);
    // }

    if (!source) {  // todo:发送STREAM NOT FOUND
        resp->add_header("Connection", "close");
        resp->set_status(404, "Not Found");
        CORE_DEBUG("could not find source:{}", source_name);
        co_await send_funcs_channel_.async_send(boost::system::error_code{},
                                                std::bind(&RtspServerSession::send_rtsp_resp, this, resp),
                                                boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        co_return false;
    }

    // 4. 判断是否需要进行桥转换
    std::shared_ptr<MediaBridge> bridge;
    if (source->get_media_type() != "rtsp{rtp[es]}") {
        bridge = source->get_or_create_bridge(source->get_media_type() + "-rtsp{rtp[es]}", publish_app,
                                              stream_name_);
        if (!bridge) {  // todo:发送FORBIDDEN
            spdlog::error("could not create bridge:{}", source->get_media_type() + "-rtsp{rtp[es]}");
            co_return false;
        }
        source = bridge->get_media_source();
    }

    play_source_ = std::dynamic_pointer_cast<RtspMediaSource>(source);
    // 5. 真的没找到
    if (!play_source_) {  // todo : reply 404
        resp->add_header("Connection", "close");
        resp->set_status(404, "Not Found");
        co_await send_funcs_channel_.async_send(boost::system::error_code{},
                                                std::bind(&RtspServerSession::send_rtsp_resp, this, resp),
                                                boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        co_return true;
    }

    rtsp_media_sink_ = std::make_shared<RtspMediaSink>(get_worker());
    rtsp_media_sink_->on_close([this, self]() { stop(); });

    int32_t try_get_play_sdp_count = 0;
    std::shared_ptr<Sdp> source_sdp;
    while (1) {
        source_sdp = play_source_->get_announce_sdp();
        if (source_sdp) {
            break;
        }

        try_get_play_sdp_count++;
        if (try_get_play_sdp_count >= 300) {
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

    if (!source_sdp) {
        resp->add_header("Connection", "close");
        resp->set_status(504, "Gateway timeout");
        co_await send_funcs_channel_.async_send(boost::system::error_code{},
                                                std::bind(&RtspServerSession::send_rtsp_resp, this, resp),
                                                boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        co_return true;
    }

    auto &source_medias = source_sdp->get_media_sdps();
    auto &describe_sdp = rtsp_media_sink_->get_describe_sdp();
    describe_sdp.set_version(0);
    describe_sdp.set_origin(source_sdp->get_origin());  // o=- get_rand64 1 IN IP4 127.0.0.1
    describe_sdp.set_session_name(session_name_);       //
    describe_sdp.set_time({0, 0});                      // t=0 0
    describe_sdp.set_tool({"mms"});
    for (auto &media : source_medias) {
        if (media.get_media() == "audio") {
            MediaSdp audio_sdp;
            audio_sdp.set_media("audio");
            audio_sdp.set_port(0);
            audio_sdp.set_port_count(1);
            audio_sdp.set_proto(media.get_proto());
            audio_sdp.set_control(media.get_control().value());
            audio_sdp.add_fmt(play_source_->get_audio_pt());

            auto &payloads = media.get_payloads();
            auto it_payload = payloads.begin();

            rtsp_media_sink_->set_audio_pt(it_payload->first);
            Payload audio_payload(it_payload->first, it_payload->second.get_encoding_name(),
                                  it_payload->second.get_clock_rate(),
                                  it_payload->second.get_encoding_params());
            audio_payload.set_fmtps(it_payload->second.get_fmtps());
            audio_sdp.add_payload(audio_payload);
            describe_sdp.add_media_sdp(audio_sdp);
        } else if (media.get_media() == "video") {
            MediaSdp video_sdp;
            video_sdp.set_media("video");
            video_sdp.set_port(0);
            video_sdp.set_port_count(1);
            video_sdp.set_proto(media.get_proto());
            video_sdp.add_fmt(play_source_->get_video_pt());
            video_sdp.set_control(media.get_control().value());
            auto &payloads = media.get_payloads();
            auto it_payload = payloads.begin();

            rtsp_media_sink_->set_video_pt(it_payload->first);
            Payload video_payload(it_payload->first, it_payload->second.get_encoding_name(),
                                  it_payload->second.get_clock_rate(),
                                  it_payload->second.get_encoding_params());
            video_payload.set_fmtps(it_payload->second.get_fmtps());
            video_sdp.add_payload(video_payload);
            describe_sdp.add_media_sdp(video_sdp);
        }
    }

    is_player_ = true;
    std::string response_sdp = describe_sdp.to_string();
    resp->add_header("Content-Base", "rtsp://" + session_name_ + "/");
    resp->add_header("Content-Type", "application/sdp");
    resp->add_header("Content-Length", std::to_string(response_sdp.size()));
    resp->set_body(response_sdp);
    co_await send_funcs_channel_.async_send(boost::system::error_code{},
                                            std::bind(&RtspServerSession::send_rtsp_resp, this, resp),
                                            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    co_return true;
}

boost::asio::awaitable<bool> RtspServerSession::process_setup_req(std::shared_ptr<RtspRequest> req) {
    boost::system::error_code ec;
    std::shared_ptr<RtspResponse> resp = std::make_shared<RtspResponse>();
    const std::string &uri = req->get_uri();
    std::vector<std::string_view> vs_url;
    boost::split(vs_url, uri, boost::is_any_of("/"));
    if (vs_url.size() != 6) {
        resp->add_header("CSeq", req->get_header("CSeq"));
        resp->add_header("Connection", "Close");
        resp->add_header("Server", "mms");
        resp->set_status(RTSP_STATUS_CODE_FORBIDDEN, "Forbidden");
        co_await send_funcs_channel_.async_send(boost::system::error_code{},
                                                std::bind(&RtspServerSession::send_rtsp_resp, this, resp),
                                                boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        co_return true;
    }

    const std::string_view &control = vs_url[5];
    if (rtsp_media_source_) {
        auto sdp = rtsp_media_source_->get_announce_sdp();
        const auto &remote_medias = sdp->get_media_sdps();
        for (const auto &media : remote_medias) {
            auto &ctrl = media.get_control();
            if (!ctrl) {
                resp->add_header("CSeq", req->get_header("CSeq"));
                resp->add_header("Connection", "Close");
                resp->add_header("Server", "mms");
                resp->set_status(RTSP_STATUS_CODE_PARAMETER_NOT_UNDERSTOOD, "Parameter Not Understood");
                co_await send_funcs_channel_.async_send(
                    boost::system::error_code{}, std::bind(&RtspServerSession::send_rtsp_resp, this, resp),
                    boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                co_return true;
            }

            if (ctrl.value().get_val() == control) {
                // media_sdp = (MediaSdp*)&media;
            }
        }
    } else if (rtsp_media_sink_) {
    }

    const std::string &transport = req->get_header("Transport");
    std::string_view tview = transport;
    std::vector<std::string_view> vs;
    boost::split(vs, tview, boost::is_any_of(";"));
    if (vs.size() < 2) {
        resp->add_header("CSeq", req->get_header("CSeq"));
        resp->add_header("Connection", "Close");
        resp->add_header("Server", "mms");
        resp->set_status(RTSP_STATUS_CODE_PARAMETER_NOT_UNDERSTOOD, "Parameter Not Understood");
        co_await send_funcs_channel_.async_send(boost::system::error_code{},
                                                std::bind(&RtspServerSession::send_rtsp_resp, this, resp),
                                                boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        co_return true;
    }

    if (vs[0] == "RTP/AVP/TCP") {
        is_tcp_transport_ = true;
    } else if (vs[0] == "RTP/AVP/UDP") {
        is_udp_transport_ = true;
    }

    if (vs[1] == "unicast") {
        is_unicast_ = true;
    } else if (vs[1] == "multicast") {
        is_multicast_ = true;
    }

    resp->add_header("CSeq", req->get_header("CSeq"));
    resp->add_header("Server", "mms");
    resp->add_header("Session", std::to_string(session_id_) + ";timeout=60");
    resp->add_header("Cache-Control", "no-cache");
    if (rtsp_media_source_) {
        resp->add_header("Transport", "RTP/AVP/TCP;unicast;interleaved=0-1;mode=\"RECORD\"");
    } else if (rtsp_media_sink_) {
        if (vs_url[5] == "streamid=0") {
            resp->add_header("Transport", "RTP/AVP/TCP;unicast;interleaved=0-1;mode=\"PLAY\"");
        } else {
            resp->add_header("Transport", "RTP/AVP/TCP;unicast;interleaved=2-3;mode=\"PLAY\"");
        }
    }

    resp->set_status(RTSP_STATUS_CODE_OK, "OK");
    co_await send_funcs_channel_.async_send(boost::system::error_code{},
                                            std::bind(&RtspServerSession::send_rtsp_resp, this, resp),
                                            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    co_return true;
}

boost::asio::awaitable<bool> RtspServerSession::process_record_req(std::shared_ptr<RtspRequest> req) {
    boost::system::error_code ec;
    std::shared_ptr<RtspResponse> resp = std::make_shared<RtspResponse>();
    resp->add_header("CSeq", req->get_header("CSeq"));
    resp->add_header("Server", "mms");
    resp->add_header("Session", std::to_string(session_id_) + ";timeout=60");
    resp->set_status(RTSP_STATUS_CODE_OK, "OK");
    co_await send_funcs_channel_.async_send(boost::system::error_code{},
                                            std::bind(&RtspServerSession::send_rtsp_resp, this, resp),
                                            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    co_return true;
}

boost::asio::awaitable<bool> RtspServerSession::process_play_req(std::shared_ptr<RtspRequest> req) {
    boost::system::error_code ec;
    auto self(shared_from_this());
    std::shared_ptr<RtspResponse> resp = std::make_shared<RtspResponse>();
    // todo: how to record 404 error to log.
    auto app = get_app();
    if (!play_source_) {  // todo : reply 404
        resp->add_header("Connection", "close");
        resp->set_status(404, "Not Found");
        co_await send_funcs_channel_.async_send(boost::system::error_code{},
                                                std::bind(&RtspServerSession::send_rtsp_resp, this, resp),
                                                boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        co_return true;
    }

    rtsp_media_sink_->set_video_pkts_cb(
        [this](std::vector<std::shared_ptr<RtpPacket>> pkts) -> boost::asio::awaitable<bool> {
            boost::system::error_code ec;
            co_await send_funcs_channel_.async_send(
                boost::system::error_code{},
                std::bind(&RtspServerSession::send_rtp_over_tcp_pkts, this, pkts),
                boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            if (ec) {
                co_return false;
            }
            co_return true;
        });

    rtsp_media_sink_->set_audio_pkts_cb(
        [this](std::vector<std::shared_ptr<RtpPacket>> pkts) -> boost::asio::awaitable<bool> {
            boost::system::error_code ec;
            co_await send_funcs_channel_.async_send(
                boost::system::error_code{},
                std::bind(&RtspServerSession::send_rtp_over_tcp_pkts, this, pkts),
                boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            if (ec) {
                co_return false;
            }
            co_return true;
        });

    play_source_->add_media_sink(rtsp_media_sink_);
    resp->add_header("CSeq", req->get_header("CSeq"));
    resp->add_header("Server", "mms");
    resp->add_header("Session", std::to_string(session_id_) + ";timeout=60");
    resp->add_header("RTP-Info", "url=rtsp://" + session_name_ + "/streamid=0,url=rtsp://" + session_name_ +
                                     "/streamid=1");
    resp->set_status(RTSP_STATUS_CODE_OK, "OK");
    co_await send_funcs_channel_.async_send(boost::system::error_code{},
                                            std::bind(&RtspServerSession::send_rtsp_resp, this, resp),
                                            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    co_return true;
}

boost::asio::awaitable<bool> RtspServerSession::process_teardown_req(std::shared_ptr<RtspRequest> req) {
    boost::system::error_code ec;
    std::shared_ptr<RtspResponse> resp = std::make_shared<RtspResponse>();
    resp->add_header("CSeq", req->get_header("CSeq"));
    resp->add_header("Server", "mms");
    resp->add_header("Session", std::to_string(session_id_) + ";timeout=60");
    resp->set_status(RTSP_STATUS_CODE_OK, "OK");
    co_await send_funcs_channel_.async_send(boost::system::error_code{},
                                            std::bind(&RtspServerSession::send_rtsp_resp, this, resp),
                                            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    co_return true;
}