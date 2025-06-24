#include "http_long_m4s_server_session.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>

#include "app/play_app.h"
#include "app/publish_app.h"
#include "base/thread/thread_worker.hpp"
#include "bridge/media_bridge.hpp"
#include "core/m4s_media_sink.hpp"
#include "core/m4s_media_source.hpp"
#include "core/source_manager.hpp"
#include "core/stream_session.hpp"
#include "log/log.h"
#include "protocol/http/http_request.hpp"
#include "protocol/http/http_response.hpp"
#include "protocol/mp4/m4s_segment.h"

using namespace mms;
HttpLongM4sServerSession::HttpLongM4sServerSession(std::shared_ptr<HttpRequest> http_req, std::shared_ptr<HttpResponse> http_resp)
    : StreamSession(http_resp->get_worker()), send_funcs_channel_(get_worker()->get_io_context(), 2048), wg_(get_worker()) {
    http_request_ = http_req;
    http_response_ = http_resp;
}

void HttpLongM4sServerSession::start() {
    auto self(std::static_pointer_cast<HttpLongM4sServerSession>(this->shared_from_this()));
    boost::asio::co_spawn(
        worker_->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            auto host = http_request_->get_header("Host");
            auto pos = host.find_first_of(":");
            if (pos != std::string::npos) {
                host = host.substr(0, pos);
            }

            set_session_info(host, http_request_->get_path_param("app"), http_request_->get_path_param("stream"));
            if (!find_and_set_app(domain_name_, app_name_, false)) {
                CORE_DEBUG("could not find conf for domain:{}, app:{}", domain_name_, app_name_);
                http_response_->add_header("Connection", "close");
                http_response_->add_header("Content-Length", "0");
                http_response_->add_header("Access-Control-Allow-Origin", "*");
                co_await http_response_->write_header(403, "Forbidden");
                stop();
                co_return;
            }

            auto play_app = std::static_pointer_cast<PlayApp>(get_app());
            auto publish_app = play_app->get_publish_app();
            if (!publish_app) {
                CORE_DEBUG("could not find publish app for domain:{}, app:{}", domain_name_, app_name_);
                http_response_->add_header("Connection", "close");
                http_response_->add_header("Content-Length", "0");
                http_response_->add_header("Access-Control-Allow-Origin", "*");
                co_await http_response_->write_header(403, "Forbidden");
                stop();
                co_return;
            }

            auto source_name = publish_app->get_domain_name() + "/" + app_name_ + "/" + http_request_->get_path_param("stream");
            auto id = http_request_->get_path_param("id");
            const std::string mp4_name = stream_name_ + "/" + id + ".m4s";
            auto source = SourceManager::get_instance().get_source(publish_app->get_domain_name(), get_app_name(), get_stream_name());
            std::shared_ptr<M4sMediaSource> mp4_source;
            if (!source) { // todo : reply 404
                CORE_DEBUG("could not find source for domain:{}, app:{}", domain_name_, app_name_);
                http_response_->add_header("Connection", "close");
                http_response_->add_header("Content-Length", "0");
                http_response_->add_header("Access-Control-Allow-Origin", "*");
                co_await http_response_->write_header(404, "Not Found");
                stop();
                co_return;
            }

            if (source->get_media_type() != "m4s") {
                auto mp4_bridge = source->get_or_create_bridge(source->get_media_type() + "-m4s", publish_app, stream_name_);
                if (!mp4_bridge) {
                    http_response_->add_header("Connection", "close");
                    http_response_->add_header("Content-Length", "0");
                    http_response_->add_header("Access-Control-Allow-Origin", "*");
                    co_await http_response_->write_header(415, "Unsupported Media Type");
                    stop();
                    co_return;
                }

                mp4_source = std::static_pointer_cast<M4sMediaSource>(mp4_bridge->get_media_source());
            }

            mp4_media_sink_ = std::make_shared<M4sMediaSink>(worker_);

            // 创建发送协程
            wg_.add(1);
            boost::asio::co_spawn(
                worker_->get_io_context(),
                [this, self]() -> boost::asio::awaitable<void> {
                    boost::system::error_code ec;
                    while (1) {
                        auto send_func = co_await send_funcs_channel_.async_receive(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                        if (ec) {
                            break;
                        }

                        bool ret = co_await send_func();
                        if (!ret) {
                            break;
                        }
                    }

                    co_return;
                },
                [this, self](std::exception_ptr exp) {
                    (void)exp;
                    wg_.done();
                    stop();
                });

            mp4_media_sink_->on_close([this, self]() { stop(); });
            mp4_media_sink_->set_combined_init_segment_cb([this, self](std::shared_ptr<Mp4Segment> seg) -> boost::asio::awaitable<bool> {
                boost::system::error_code ec;
                co_await send_funcs_channel_.async_send(boost::system::error_code{}, std::bind(&HttpLongM4sServerSession::send_fmp4_seg, this, seg),
                                                        boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                if (ec) {
                    co_return false;
                }
                co_return true;
            });


            mp4_media_sink_->set_audio_mp4_segment_cb([this, self](std::shared_ptr<Mp4Segment> seg) -> boost::asio::awaitable<bool> {
                boost::system::error_code ec;
                co_await send_funcs_channel_.async_send(boost::system::error_code{}, std::bind(&HttpLongM4sServerSession::send_fmp4_seg, this, seg),
                                                        boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                if (ec) {
                    co_return false;
                }
                co_return true;
            });

            mp4_media_sink_->set_video_mp4_segment_cb([this, self](std::shared_ptr<Mp4Segment> seg) -> boost::asio::awaitable<bool> {
                boost::system::error_code ec;
                co_await send_funcs_channel_.async_send(boost::system::error_code{}, std::bind(&HttpLongM4sServerSession::send_fmp4_seg, this, seg),
                                                        boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                if (ec) {
                    co_return false;
                }
                co_return true;
            });

            http_response_->add_header("Content-Type", "video/mp4");
            http_response_->add_header("Connection", "close");
            http_response_->add_header("Access-Control-Allow-Origin", "*");
            if (!co_await http_response_->write_header(200, "Ok")) {
                stop();
                co_return;
            }

            mp4_source->add_media_sink(mp4_media_sink_);
            co_return;
        },
        boost::asio::detached);
}

boost::asio::awaitable<bool> HttpLongM4sServerSession::send_fmp4_seg(std::shared_ptr<Mp4Segment> seg) {
    auto data = seg->get_used_buf();
    if (!co_await http_response_->write_data(data)) {
        stop();
        co_return false;
    }
    CORE_DEBUG("send data bytes:{}", data.size());
    co_return true;
}

void HttpLongM4sServerSession::stop() {
    // todo: how to record 404 error to log.
    if (closed_.test_and_set()) {
        return;
    }

    auto self(shared_from_this());
    boost::asio::co_spawn(
        worker_->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            CORE_DEBUG("closing HttpLongM4sServerSession...");
            send_funcs_channel_.close();
            co_await wg_.wait();

            http_response_->close();
            if (mp4_media_sink_) {
                auto source = mp4_media_sink_->get_source();
                if (source) {
                    source->remove_media_sink(mp4_media_sink_);
                    mp4_media_sink_->on_close({});
                    mp4_media_sink_->set_combined_init_segment_cb({});
                    mp4_media_sink_->set_audio_mp4_segment_cb({});
                    mp4_media_sink_->set_video_mp4_segment_cb({});
                    mp4_media_sink_->close();
                }

                mp4_media_sink_ = nullptr;
            }
            CORE_DEBUG("HttpLongM4sServerSession closed");
            co_return;
        },
        boost::asio::detached);
}