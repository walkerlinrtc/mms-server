#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>

#include "http_long_mp4_server_session.hpp"

#include "log/log.h"
#include "app/play_app.h"
#include "app/publish_app.h"

#include "protocol/http/http_request.hpp"
#include "protocol/http/http_response.hpp"
#include "protocol/mp4/mp4_segment.h"

#include "base/thread/thread_worker.hpp"
#include "core/stream_session.hpp"

#include "core/mp4_media_source.hpp"
#include "core/mp4_media_sink.hpp"

#include "bridge/media_bridge.hpp"
#include "core/source_manager.hpp"

using namespace mms;
HttpLongMp4ServerSession::HttpLongMp4ServerSession(std::shared_ptr<HttpRequest> http_req, std::shared_ptr<HttpResponse> http_resp):
            StreamSession(http_resp->get_worker()), send_funcs_channel_(get_worker()->get_io_context(), 2048), wg_(get_worker()) {
    http_request_ = http_req;
    http_response_ = http_resp;
}

HttpLongMp4ServerSession::~HttpLongMp4ServerSession() {

}

void HttpLongMp4ServerSession::service() {
    auto self(std::static_pointer_cast<HttpLongMp4ServerSession>(this->shared_from_this()));
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
        auto id = http_request_->get_path_param("id");
        const std::string mp4_name = stream_name_ + "/" + id + ".m4s";
        auto source = SourceManager::get_instance().get_source(get_domain_name(), get_app_name(), get_stream_name());
        std::shared_ptr<Mp4MediaSource> mp4_source;
        if (!source) {//todo : reply 404
            CORE_DEBUG("could not find source for domain:{}, app:{}", domain_name_, app_name_);
            http_response_->add_header("Connection", "close");
            http_response_->add_header("Content-Length", "0");
            http_response_->add_header("Access-Control-Allow-Origin", "*");
            co_await http_response_->write_header(404, "Not Found");
            close();
            co_return;
        }

        if (source->get_media_type() != "mp4") {
            auto mp4_bridge = source->get_or_create_bridge(source->get_media_type() + "-mp4", publish_app, stream_name_);
            if (!mp4_bridge) {
                http_response_->add_header("Connection", "close");
                http_response_->add_header("Content-Length", "0");
                http_response_->add_header("Access-Control-Allow-Origin", "*");
                co_await http_response_->write_header(415, "Unsupported Media Type");
                close();
                co_return;
            }

            mp4_source = std::static_pointer_cast<Mp4MediaSource>(mp4_bridge->get_media_source());
        } 

        mp4_media_sink_ = std::make_shared<Mp4MediaSink>(worker_);

        // 创建发送协程
        boost::asio::co_spawn(worker_->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
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
        }, [this, self](std::exception_ptr exp) {
            (void)exp;
            wg_.done();
            close();
        });
        
        mp4_media_sink_->set_audio_init_segment_cb([this, self](std::shared_ptr<Mp4Segment> seg)->boost::asio::awaitable<bool> {
            boost::system::error_code ec;
            co_await send_funcs_channel_.async_send(boost::system::error_code{}, std::bind(&HttpLongMp4ServerSession::send_fmp4_seg, this, seg), boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            if (ec) {
                co_return false;
            }
        });

        mp4_media_sink_->set_video_init_segment_cb([this, self](std::shared_ptr<Mp4Segment> seg)->boost::asio::awaitable<bool> {
            boost::system::error_code ec;
            co_await send_funcs_channel_.async_send(boost::system::error_code{}, std::bind(&HttpLongMp4ServerSession::send_fmp4_seg, this, seg), boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            if (ec) {
                co_return false;
            }
        });

        mp4_media_sink_->set_audio_mp4_segment_cb([this, self](std::shared_ptr<Mp4Segment> seg)->boost::asio::awaitable<bool> {
            boost::system::error_code ec;
            co_await send_funcs_channel_.async_send(boost::system::error_code{}, std::bind(&HttpLongMp4ServerSession::send_fmp4_seg, this, seg), boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            if (ec) {
                co_return false;
            }
        });

        mp4_media_sink_->set_video_mp4_segment_cb([this, self](std::shared_ptr<Mp4Segment> seg)->boost::asio::awaitable<bool> {
            boost::system::error_code ec;
            co_await send_funcs_channel_.async_send(boost::system::error_code{}, std::bind(&HttpLongMp4ServerSession::send_fmp4_seg, this, seg), boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            if (ec) {
                co_return false;
            }
        });

        http_response_->add_header("Content-Type", "video/mp4");
        http_response_->add_header("Connection", "close");
        http_response_->add_header("Access-Control-Allow-Origin", "*");
        if (!co_await http_response_->write_header(200, "Ok")) {
            close();
            co_return;
        }

        mp4_source->add_media_sink(mp4_media_sink_);
        co_return;
    }, boost::asio::detached);
}

boost::asio::awaitable<bool> HttpLongMp4ServerSession::send_fmp4_seg(std::shared_ptr<Mp4Segment> seg) {
    auto data = seg->get_used_buf();
    if (!co_await http_response_->write_data(data)) {
        close();
        co_return false;
    }
    co_return true;
}

void HttpLongMp4ServerSession::close() {
    // todo: how to record 404 error to log.
    if (closed_.test_and_set()) {
        return;
    }
    
    auto self(shared_from_this());
    boost::asio::co_spawn(worker_->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        CORE_DEBUG("closing HttpLongTsServerSession...");
        send_funcs_channel_.close();
        co_await wg_.wait();
        
        http_response_->close();
        if (mp4_media_sink_) {
            auto source = mp4_media_sink_->get_source();
            if (source) {
                source->remove_media_sink(mp4_media_sink_);
                mp4_media_sink_->close();
            }

            mp4_media_sink_ = nullptr;
        }
        CORE_DEBUG("HttpLongTsServerSession closed");
        co_return;
    }, boost::asio::detached);
}