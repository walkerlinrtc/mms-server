#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>

#include "http_ts_server_session.hpp"

#include "log/log.h"
#include "app/play_app.h"
#include "app/publish_app.h"

#include "protocol/http/http_request.hpp"
#include "protocol/http/http_response.hpp"
#include "protocol/ts/ts_segment.hpp"

#include "base/thread/thread_worker.hpp"
#include "core/stream_session.hpp"

#include "core/ts_media_source.hpp"
#include "core/hls_live_media_source.hpp"
#include "bridge/media_bridge.hpp"
#include "core/source_manager.hpp"

using namespace mms;
HttpTsServerSession::HttpTsServerSession(std::shared_ptr<HttpRequest> http_req, std::shared_ptr<HttpResponse> http_resp):StreamSession(http_resp->get_worker()) {
    http_request_ = http_req;
    http_response_ = http_resp;
}

HttpTsServerSession::~HttpTsServerSession() {

}

void HttpTsServerSession::service() {
    auto self(std::static_pointer_cast<HttpTsServerSession>(this->shared_from_this()));
    boost::asio::co_spawn(worker_->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
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
            close();
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
            close();
            co_return;
        }

        auto source_name = publish_app->get_domain_name() + "/" + app_name_ + "/" + http_request_->get_path_param("stream");
        const std::string ts_name = stream_name_ + "/" + http_request_->get_path_param("id") + ".ts";
        auto source = SourceManager::get_instance().get_source(get_domain_name(), get_app_name(), get_stream_name());
        std::shared_ptr<HlsLiveMediaSource> hls_source;
        if (!source) {//todo : reply 404
            CORE_DEBUG("could not find source for domain:{}, app:{}", domain_name_, app_name_);
            http_response_->add_header("Connection", "close");
            http_response_->add_header("Content-Length", "0");
            http_response_->add_header("Access-Control-Allow-Origin", "*");
            co_await http_response_->write_header(404, "Not Found");
            close();
            co_return;
        } else {
            if (source->get_media_type() != "hls") {
                auto ts_bridge = source->get_or_create_bridge(source->get_media_type() + "-ts", publish_app, stream_name_);
                if (!ts_bridge) {
                    http_response_->add_header("Connection", "close");
                    http_response_->add_header("Content-Length", "0");
                    http_response_->add_header("Access-Control-Allow-Origin", "*");
                    co_await http_response_->write_header(415, "Unsupported Media Type");
                    close();
                    co_return;
                }

                auto ts_source = std::static_pointer_cast<TsMediaSource>(ts_bridge->get_media_source());
                auto hls_bridge = ts_source->get_or_create_bridge(ts_source->get_media_type() + "-hls", publish_app, stream_name_);
                if (!hls_bridge) {
                    http_response_->add_header("Connection", "close");
                    http_response_->add_header("Content-Length", "0");
                    http_response_->add_header("Access-Control-Allow-Origin", "*");
                    co_await http_response_->write_header(415, "Unsupported Media Type");
                    close();
                    co_return;
                }
                hls_source = std::static_pointer_cast<HlsLiveMediaSource>(hls_bridge->get_media_source());
            } else {
                hls_source = std::static_pointer_cast<HlsLiveMediaSource>(source);
            }

            hls_source->update_last_access_time();
            auto ts_seg = hls_source->get_ts_segment(ts_name);
            if (!ts_seg) {
                http_response_->add_header("Connection", "close");
                http_response_->add_header("Content-Length", "0");
                http_response_->add_header("Access-Control-Allow-Origin", "*");
                co_await http_response_->write_header(404, "Not Found");
                close();
                co_return;
            }
            
            auto ts_data = ts_seg->get_ts_data();
            http_response_->add_header("Content-Type", "video/MP2T");
            http_response_->add_header("Connection", "close");
            http_response_->add_header("Access-Control-Allow-Origin", "*");
            http_response_->add_header("Content-Length", std::to_string(ts_data.size()));
            if (!co_await http_response_->write_header(200, "Ok")) {
                close();
                co_return;
            }

            co_await http_response_->write_data(ts_data);
        }

        co_return;
    }, boost::asio::detached);
}

void HttpTsServerSession::close() {
    // todo: how to record 404 error to log.
    if (closed_.test_and_set()) {
        return;
    }
    http_response_->close();
}