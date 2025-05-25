#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>

#include "http_m3u8_server_session.hpp"
#include "base/thread/thread_worker.hpp"

#include "core/ts_media_source.hpp"
#include "core/hls_live_media_source.hpp"
#include "core/source_manager.hpp"

#include "protocol/http/http_request.hpp"
#include "protocol/http/http_response.hpp"

#include "core/source_manager.hpp"
#include "app/play_app.h"
#include "app/publish_app.h"
#include "config/app_config.h"

#include "bridge/media_bridge.hpp"

using namespace mms;

HttpM3u8ServerSession::HttpM3u8ServerSession(std::shared_ptr<HttpRequest> http_req, std::shared_ptr<HttpResponse> http_resp):StreamSession(http_resp->get_worker()) {
    http_request_ = http_req;
    http_response_ = http_resp;
}

HttpM3u8ServerSession::~HttpM3u8ServerSession() {
}

void HttpM3u8ServerSession::service() {
    auto self(std::static_pointer_cast<HttpM3u8ServerSession>(this->shared_from_this()));
    boost::asio::co_spawn(worker_->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        auto host = http_request_->get_header("Host");
        auto pos = host.find_first_of(":");
        if (pos != std::string::npos) {
            host = host.substr(0, pos);
        }

        set_session_info(host, http_request_->get_path_param("app"), http_request_->get_path_param("stream"));
        if (!find_and_set_app(domain_name_, app_name_, false)) {
            CORE_DEBUG("could not find play app for domain:{}, app:{}", domain_name_, app_name_);
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
        // 1.本机查找
        auto source = SourceManager::get_instance().get_source(publish_app->get_domain_name(), get_app_name(), get_stream_name());
        if (!source) {// 2.本地配置查找外部回源
            source = co_await publish_app->find_media_source(self);
        }
        // 3.到media center中心查找
        // if (!source) {
        //     source = co_await MediaCenterManager::get_instance().find_and_create_pull_media_source(self);
        // }

        std::shared_ptr<HlsLiveMediaSource> hls_source;
        if (!source) {//todo : reply 404
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
                hls_source->set_source_info(domain_name_, app_name_, stream_name_);
            } else {
                hls_source = std::static_pointer_cast<HlsLiveMediaSource>(source);
            }

            int try_count = 0;
            while (try_count <= 400) {
                hls_source->update_last_access_time();
                auto status = hls_source->get_status();
                bool ret = co_await process_source_status(status);
                if (!ret) {
                    co_return;
                }
                std::string m3u8 = hls_source->get_m3u8();
                if (m3u8.empty()) {
                    try_count++;
                    boost::system::error_code ec;
                    boost::asio::steady_timer timer(worker_->get_io_context());
                    timer.expires_from_now(std::chrono::milliseconds(100));
                    co_await timer.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                    continue;
                }

                http_response_->add_header("Connection", "close");
                http_response_->add_header("Content-Type", "application/vnd.apple.mpegurl");
                http_response_->add_header("Content-Length", std::to_string(m3u8.size()));
                http_response_->add_header("Access-Control-Allow-Origin", "*");
                if (!(co_await http_response_->write_header(200, "OK"))) {
                    close();
                    co_return;
                }
                co_await http_response_->write_data((uint8_t*)m3u8.data(), m3u8.size());
                co_return;
            }

            http_response_->add_header("Connection", "close");
            http_response_->add_header("Content-Length", "0");
            http_response_->add_header("Access-Control-Allow-Origin", "*");
            co_await http_response_->write_header(404, "Not Found");
            close();
        }

        co_return;
    }, boost::asio::detached);
}

boost::asio::awaitable<bool> HttpM3u8ServerSession::process_source_status(SourceStatus status) {
    if (status == E_SOURCE_STATUS_NOT_FOUND) {
        http_response_->add_header("Connection", "close");
        http_response_->add_header("Content-Type", "application/vnd.apple.mpegurl");
        http_response_->add_header("Access-Control-Allow-Origin", "*");
        if (!(co_await http_response_->write_header(404, "Not Found"))) {
            close();
            co_return false;
        }
        co_return false;
    } else if (status == E_SOURCE_STATUS_UNAUTHORIZED) {
        http_response_->add_header("Connection", "close");
        http_response_->add_header("Content-Type", "application/vnd.apple.mpegurl");
        http_response_->add_header("Access-Control-Allow-Origin", "*");
        if (!(co_await http_response_->write_header(401, "Unauthorized"))) {
            close();
            co_return false;
        }
        co_return false;
    } else if (status == E_SOURCE_STATUS_FORBIDDEN) {
        http_response_->add_header("Connection", "close");
        http_response_->add_header("Content-Type", "application/vnd.apple.mpegurl");
        http_response_->add_header("Access-Control-Allow-Origin", "*");
        if (!(co_await http_response_->write_header(403, "Forbidden"))) {
            close();
            co_return false;
        }
        co_return false;
    } else {
        http_response_->add_header("Connection", "close");
        http_response_->add_header("Content-Type", "application/vnd.apple.mpegurl");
        http_response_->add_header("Access-Control-Allow-Origin", "*");
        if (!(co_await http_response_->write_header(500, "Gateway timeout"))) {
            close();
            co_return false;
        }
        co_return false;
    }
    co_return true;
}

void HttpM3u8ServerSession::close() {
    // todo: how to record 404 error to log.
    if (closed_.test_and_set()) {
        return;
    }
    http_response_->close();
    return;
}