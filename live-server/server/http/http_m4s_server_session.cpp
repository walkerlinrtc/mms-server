#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>

#include "http_m4s_server_session.hpp"

#include "log/log.h"
#include "app/play_app.h"
#include "app/publish_app.h"

#include "protocol/http/http_request.hpp"
#include "protocol/http/http_response.hpp"
#include "protocol/mp4/m4s_segment.h"

#include "base/thread/thread_worker.hpp"
#include "core/stream_session.hpp"

#include "core/m4s_media_source.hpp"
#include "core/mpd_live_media_source.hpp"
#include "bridge/media_bridge.hpp"
#include "core/source_manager.hpp"

using namespace mms;
HttpM4sServerSession::HttpM4sServerSession(std::shared_ptr<HttpRequest> http_req, std::shared_ptr<HttpResponse> http_resp):StreamSession(http_resp->get_worker()) {
    http_request_ = http_req;
    http_response_ = http_resp;
}

HttpM4sServerSession::~HttpM4sServerSession() {

}

void HttpM4sServerSession::start() {
    auto self(std::static_pointer_cast<HttpM4sServerSession>(this->shared_from_this()));
    boost::asio::co_spawn(worker_->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        spdlog::info("http request m4s, id:{}", http_request_->get_path_param("id"));
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
        std::shared_ptr<MpdLiveMediaSource> mpd_source;
        if (!source) {//todo : reply 404
            CORE_DEBUG("could not find source for domain:{}, app:{}", domain_name_, app_name_);
            http_response_->add_header("Connection", "close");
            http_response_->add_header("Content-Length", "0");
            http_response_->add_header("Access-Control-Allow-Origin", "*");
            co_await http_response_->write_header(404, "Not Found");
            stop();
            co_return;
        } else {
            if (source->get_media_type() != "mpd") {
                auto mp4_bridge = source->get_or_create_bridge(source->get_media_type() + "-m4s", publish_app, stream_name_);
                if (!mp4_bridge) {
                    http_response_->add_header("Connection", "close");
                    http_response_->add_header("Content-Length", "0");
                    http_response_->add_header("Access-Control-Allow-Origin", "*");
                    co_await http_response_->write_header(415, "Unsupported Media Type");
                    stop();
                    co_return;
                }

                auto mp4_source = std::static_pointer_cast<M4sMediaSource>(mp4_bridge->get_media_source());
                auto mpd_bridge = mp4_source->get_or_create_bridge(mp4_source->get_media_type() + "-mpd", publish_app, stream_name_);
                if (!mpd_bridge) {
                    http_response_->add_header("Connection", "close");
                    http_response_->add_header("Content-Length", "0");
                    http_response_->add_header("Access-Control-Allow-Origin", "*");
                    co_await http_response_->write_header(415, "Unsupported Media Type");
                    stop();
                    co_return;
                }
                mpd_source = std::static_pointer_cast<MpdLiveMediaSource>(mpd_bridge->get_media_source());
            } else {
                mpd_source = std::static_pointer_cast<MpdLiveMediaSource>(source);
            }

            mpd_source->update_last_access_time();
            
            auto mp4_seg = mpd_source->get_mp4_segment(id + ".m4s");
            if (!mp4_seg) {
                http_response_->add_header("Connection", "close");
                http_response_->add_header("Content-Length", "0");
                http_response_->add_header("Access-Control-Allow-Origin", "*");
                co_await http_response_->write_header(404, "Not Found");
                stop();
                co_return;
            }
            
            auto mp4_data = mp4_seg->get_used_buf();
            if (id.starts_with("video")) {
                http_response_->add_header("Content-Type", "video/iso.segment");
            } else if (id.starts_with("audio")) {
                http_response_->add_header("Content-Type", "audio/iso.segment");
            }
            
            http_response_->add_header("Connection", "close");
            http_response_->add_header("Access-Control-Allow-Origin", "*");
            http_response_->add_header("Content-Length", std::to_string(mp4_data.size()));
            if (!co_await http_response_->write_header(200, "Ok")) {
                stop();
                co_return;
            }

            co_await http_response_->write_data(mp4_data);
        }

        co_return;
    }, boost::asio::detached);
}

void HttpM4sServerSession::stop() {
    // todo: how to record 404 error to log.
    if (closed_.test_and_set()) {
        return;
    }
    http_response_->close();
}