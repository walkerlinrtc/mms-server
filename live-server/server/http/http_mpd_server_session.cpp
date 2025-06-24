#include "http_mpd_server_session.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include "app/play_app.h"
#include "app/publish_app.h"
#include "base/thread/thread_worker.hpp"
#include "bridge/media_bridge.hpp"
#include "config/app_config.h"
#include "core/m4s_media_source.hpp"
#include "core/mpd_live_media_source.hpp"
#include "core/source_manager.hpp"
#include "protocol/http/http_request.hpp"
#include "protocol/http/http_response.hpp"


using namespace mms;

HttpMpdServerSession::HttpMpdServerSession(std::shared_ptr<HttpRequest> http_req,
                                           std::shared_ptr<HttpResponse> http_resp)
    : StreamSession(http_resp->get_worker()) {
    http_request_ = http_req;
    http_response_ = http_resp;
}

HttpMpdServerSession::~HttpMpdServerSession() {}

void HttpMpdServerSession::start() {
    auto self(std::static_pointer_cast<HttpMpdServerSession>(this->shared_from_this()));
    boost::asio::co_spawn(
        worker_->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            std::string domain = http_request_->get_query_param("domain");
            if (domain.empty()) {
                domain = http_request_->get_header("Host");
                auto pos = domain.find(":");
                if (pos != std::string::npos) {
                    domain = domain.substr(0, pos);
                }
            }

            set_session_info(domain, http_request_->get_path_param("app"),
                             http_request_->get_path_param("stream"));
            if (!find_and_set_app(domain_name_, app_name_, false)) {
                CORE_DEBUG("could not find play app for domain:{}, app:{}", domain_name_, app_name_);
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
            auto source_name = publish_app->get_domain_name() + "/" + app_name_ + "/" +
                               http_request_->get_path_param("stream");
            // 1.本机查找
            auto source = SourceManager::get_instance().get_source(publish_app->get_domain_name(), get_app_name(),
                                                                   get_stream_name());
            if (!source) {  // 2.本地配置查找外部回源
                source = co_await publish_app->find_media_source(self);
            }
            // 3.到media center中心查找
            // if (!source) {
            //     source = co_await
            //     MediaCenterManager::get_instance().find_and_create_pull_media_source(self);
            // }

            std::shared_ptr<MpdLiveMediaSource> mpd_source;
            if (!source) {  // todo : reply 404
                http_response_->add_header("Connection", "close");
                http_response_->add_header("Content-Length", "0");
                http_response_->add_header("Access-Control-Allow-Origin", "*");
                co_await http_response_->write_header(404, "Not Found");
                stop();
                co_return;
            } else {
                if (source->get_media_type() != "mpd") {
                    auto mp4_bridge = source->get_or_create_bridge(source->get_media_type() + "-m4s",
                                                                   publish_app, stream_name_);
                    if (!mp4_bridge) {
                        http_response_->add_header("Connection", "close");
                        http_response_->add_header("Content-Length", "0");
                        http_response_->add_header("Access-Control-Allow-Origin", "*");
                        co_await http_response_->write_header(415, "Unsupported Media Type");
                        stop();
                        co_return;
                    }

                    auto m4s_source =
                        std::static_pointer_cast<M4sMediaSource>(mp4_bridge->get_media_source());
                    if (!m4s_source) {
                        stop();
                        co_return;
                    }
                    auto mpd_bridge = m4s_source->get_or_create_bridge(m4s_source->get_media_type() + "-mpd",
                                                                       publish_app, stream_name_);
                    if (!mpd_bridge) {
                        http_response_->add_header("Connection", "close");
                        http_response_->add_header("Content-Length", "0");
                        http_response_->add_header("Access-Control-Allow-Origin", "*");
                        co_await http_response_->write_header(415, "Unsupported Media Type");
                        stop();
                        co_return;
                    }
                    mpd_source = std::static_pointer_cast<MpdLiveMediaSource>(mpd_bridge->get_media_source());
                    mpd_source->set_source_info(domain_name_, app_name_, stream_name_);
                } else {
                    mpd_source = std::static_pointer_cast<MpdLiveMediaSource>(source);
                }

                int try_count = 0;
                while (try_count <= 400) {
                    mpd_source->update_last_access_time();
                    std::string mpd = mpd_source->get_mpd();
                    if (mpd.empty()) {
                        try_count++;
                        boost::system::error_code ec;
                        boost::asio::steady_timer timer(worker_->get_io_context());
                        timer.expires_after(std::chrono::milliseconds(100));
                        co_await timer.async_wait(
                            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                        continue;
                    }

                    http_response_->add_header("Connection", "close");
                    http_response_->add_header("Content-Type", "application/dash+xml");
                    http_response_->add_header("Content-Length", std::to_string(mpd.size()));
                    http_response_->add_header("Access-Control-Allow-Origin", "*");
                    if (!(co_await http_response_->write_header(200, "OK"))) {
                        stop();
                        co_return;
                    }
                    co_await http_response_->write_data((uint8_t*)mpd.data(), mpd.size());
                    co_return;
                }

                http_response_->add_header("Connection", "close");
                http_response_->add_header("Content-Length", "0");
                http_response_->add_header("Access-Control-Allow-Origin", "*");
                co_await http_response_->write_header(404, "Not Found");
                stop();
            }

            co_return;
        },
        boost::asio::detached);
}

void HttpMpdServerSession::stop() {
    // todo: how to record 404 error to log.
    if (closed_.test_and_set()) {
        return;
    }
    http_response_->close();
    return;
}