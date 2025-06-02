#include "http_flv_server_session.hpp"
#include "http_server_session.hpp"

#include "core/rtmp_media_sink.hpp"
#include "core/rtmp_media_source.hpp"
#include "core/media_event.hpp"
#include "core/source_manager.hpp"
#include "core/flv_media_sink.hpp"
#include "config/app_config.h"

#include "app/app.h"
#include "app/play_app.h"
#include "app/publish_app.h"
#include "config/cdn/origin_pull_config.h"
#include "config/app_config.h"
#include "bridge/media_bridge.hpp"

#include "log/log.h"

using namespace mms;
HttpFlvServerSession::HttpFlvServerSession(std::shared_ptr<HttpRequest> http_req, std::shared_ptr<HttpResponse> http_resp) : StreamSession(http_resp->get_worker()), 
                                                                                                                send_funcs_channel_(get_worker()->get_io_context()),
                                                                                                                alive_timeout_timer_(get_worker()->get_io_context()),
                                                                                                                wg_(get_worker()) {
    http_request_ = http_req;
    http_response_ = http_resp;
    send_bufs_.reserve(20);
    set_session_type("http-flv");
}

HttpFlvServerSession::~HttpFlvServerSession() {
    CORE_DEBUG("destroy HttpFlvServerSession");
}

void HttpFlvServerSession::start_send_coroutine() {
    // 创建发送协程
    auto self(shared_from_this());
    wg_.add(1);
    boost::asio::co_spawn(worker_->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        boost::system::error_code ec;
        while (1) {
            auto send_func = co_await send_funcs_channel_.async_receive(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            if (ec) {
                break;
            }
            update_active_timestamp();
            bool ret = co_await send_func();
            if (!ret) {
                break;
            }
        }
        
        co_return;
    }, [this, self](std::exception_ptr exp) {
        (void)exp;
        close(true);
        wg_.done();
    });
}

void HttpFlvServerSession::start_alive_checker() {
    auto self(shared_from_this());
    wg_.add(1);
    boost::asio::co_spawn(worker_->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        boost::system::error_code ec;
        while (1) {
            alive_timeout_timer_.expires_from_now(std::chrono::seconds(5));
            co_await alive_timeout_timer_.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            if (ec) {
                co_return;
            }
            int64_t now_ms = Utils::get_current_ms();
            if (now_ms - last_active_time_ >= 50000) {//5秒没数据，超时
                co_return;
            }
            CORE_DEBUG("session:{} is alive", get_session_name());
        }
        co_return;
    }, [this, self](std::exception_ptr exp) {
        (void)exp;
        close();
        wg_.done();
    });
}

void HttpFlvServerSession::update_active_timestamp() {
    last_active_time_ = Utils::get_current_ms();
}

void HttpFlvServerSession::service() {
    CORE_INFO("start service HttpFlvServerSession");
    auto self(std::static_pointer_cast<HttpFlvServerSession>(shared_from_this()));
    boost::asio::co_spawn(worker_->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        auto & query_params = http_request_->get_query_params();
        for (auto & p : query_params) {
            set_param(p.first, p.second);
        }
    
        std::string domain = http_request_->get_query_param("domain");
        if (domain.empty()) {
            domain = http_request_->get_header("Host");
            auto pos = domain.find(":");
            if (pos != std::string::npos) {
                domain = domain.substr(0, pos);
            }
        }
        
        set_session_info(domain, http_request_->get_path_param("app"), http_request_->get_path_param("stream"));
        if (!find_and_set_app(domain_name_, app_name_, false)) {
            CORE_ERROR("could not find config for domain:{}, app:{}", domain_name_, app_name_);
            http_response_->add_header("Content-Type", "video/x-flv");
            http_response_->add_header("Connection", "Close");
            co_await http_response_->write_header(403, "Forbidden");
            close(true);
            co_return;
        } 

        auto play_app = std::static_pointer_cast<PlayApp>(get_app());
        APP_TRACE(app_, "{}:{} play http-flv start", id_, session_name_);
        CORE_DEBUG("{}:{} play http-flv start", id_, session_name_);
        
        auto err = co_await play_app->on_play(self);
        if (err.code != 0) {
            CORE_ERROR("play auth check failed, reason:{}", err.msg);
            http_response_->add_header("Content-Type", "video/x-flv");
            http_response_->add_header("Connection", "Close");
            co_await http_response_->write_header(403, "Forbidden");
            close(true);
            co_return;
        }
        auto publish_app = play_app->get_publish_app();
        // 1.本机查找
        auto source_name = publish_app->get_domain_name() + "/" + app_name_ + "/" + http_request_->get_path_param("stream");
        auto source = SourceManager::get_instance().get_source(publish_app->get_domain_name(), get_app_name(), get_stream_name());
        auto endpoint = this->get_param("mms-endpoint");
        if (endpoint && endpoint.value().get() == "1") {
            // if (!source) {
            //     co_await MediaCenterClient::get_instance().notify_not_found(self);
            //     http_response_->add_header("Content-Type", "video/x-flv");
            //     http_response_->add_header("Connection", "Close");
            //     co_await http_response_->write_header(404, "Not Found");
            //     close();
            //     co_return;
            // }
        }
        
        // 3. 本地配置查找外部回源
        if (!source) {
            source = co_await publish_app->find_media_source(self);
        }
        // 3. 到media center中心查找
        // if (!source) {
        //     source = co_await MediaCenterClient::get_instance().find_and_create_pull_media_source(self);
        // }

        if (!source) {
            //真找不到源流了，应该是没在播
            http_response_->add_header("Content-Type", "video/x-flv");
            http_response_->add_header("Connection", "Close");
            co_await http_response_->write_header(404, "Not Found");
            close(true);
            co_return;
        }

        // 判断是否需要进行桥转换
        std::shared_ptr<MediaBridge> bridge;
        if (source->get_media_type() != "flv") {// 尝试通过桥来创建
            bridge = source->get_or_create_bridge(source->get_media_type() + "-flv", publish_app, stream_name_);
            if (!bridge) {
                http_response_->add_header("Connection", "close");
                if (!(co_await http_response_->write_header(415, "Unsupported Media Type"))) {
                }
                close(true);
                co_return;
            }
            source = bridge->get_media_source();
        }

        flv_media_sink_ = std::make_shared<FlvMediaSink>(get_worker());
        // 关闭flv
        flv_media_sink_->on_close([this, self]() {
            close();
        });
        // 事件处理
        flv_media_sink_->set_on_source_status_changed_cb([this, self](SourceStatus status)->boost::asio::awaitable<void> {
            co_return co_await process_source_status(status);
        });
        source->add_media_sink(flv_media_sink_);
        co_return;
    }, boost::asio::detached);
}

boost::asio::awaitable<void> HttpFlvServerSession::process_source_status(SourceStatus status) {
    auto self(shared_from_this());
    if (status == E_SOURCE_STATUS_OK) {
        if (!has_send_http_header_) {
            has_send_http_header_ = true;
            //找到源流，先发http送头部
            http_response_->add_header("Content-Type", "video/x-flv");
            http_response_->add_header("Connection", "Keep-Alive");
            http_response_->add_header("Access-Control-Allow-Origin", "*");
            if (!(co_await http_response_->write_header(200, "OK"))) {
                close(true);
                co_return;
            }
        }
        // 发送完头部后，再开发送flv的tag
        start_send_coroutine();
        flv_media_sink_->on_flv_tag([this, self](std::vector<std::shared_ptr<FlvTag>> & flv_tags)->boost::asio::awaitable<bool> {
            boost::system::error_code ec;
            if (flv_tags.size() <= 0) {
                co_return true;
            }

            co_await send_funcs_channel_.async_send(boost::system::error_code{}, std::bind(&HttpFlvServerSession::send_flv_tags, this, flv_tags), boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            if (ec) {
                co_return false;
            }
            co_return true;
        });
    } else if (status == E_SOURCE_STATUS_NOT_FOUND) {
        if (!has_send_http_header_) {
            has_send_http_header_ = true;
            http_response_->add_header("Content-Type", "video/x-flv");
            http_response_->add_header("Connection", "Keep-Alive");
            http_response_->add_header("Access-Control-Allow-Origin", "*");
            if (!(co_await http_response_->write_header(404, "Not Found"))) {
                close(true);
                co_return;
            }
        }
    } else if (status == E_SOURCE_STATUS_FORBIDDEN) {
        if (!has_send_http_header_) {
            has_send_http_header_ = true;
            http_response_->add_header("Content-Type", "video/x-flv");
            http_response_->add_header("Connection", "Keep-Alive");
            http_response_->add_header("Access-Control-Allow-Origin", "*");
            if (!(co_await http_response_->write_header(403, "Forbidden"))) {
                close(true);
                co_return;
            }
        }
    } else if (status == E_SOURCE_STATUS_UNAUTHORIZED) {
        if (!has_send_http_header_) {
            has_send_http_header_ = true;
            http_response_->add_header("Content-Type", "video/x-flv");
            http_response_->add_header("Connection", "Keep-Alive");
            http_response_->add_header("Access-Control-Allow-Origin", "*");
            if (!(co_await http_response_->write_header(401, "Unauthorized"))) {
                close(true);
                co_return;
            }
        }
    } else {
        if (!has_send_http_header_) {
            has_send_http_header_ = true;
            http_response_->add_header("Content-Type", "video/x-flv");
            http_response_->add_header("Connection", "Keep-Alive");
            http_response_->add_header("Access-Control-Allow-Origin", "*");
            if (!(co_await http_response_->write_header(504, "Gateway Timeout"))) {
                close(true);
                co_return;
            }
        }
    }
    co_return;
}

boost::asio::awaitable<bool> HttpFlvServerSession::send_flv_tags(std::vector<std::shared_ptr<FlvTag>> tags) {
    if (!has_write_flv_header_) {
        // 再发flv头部
        auto source = flv_media_sink_->get_source();//SourceManager::get_instance().get_source(get_session_name());
        uint8_t flv_header[9];
        FlvHeader header;
        header.flag.flags.audio = source->has_audio()?1:0;
        header.flag.flags.video = source->has_video()?1:0;
        auto consumed = header.encode(flv_header, 9);
        if (!(co_await http_response_->write_data(std::string_view((char*)flv_header, consumed)))) {
            close(true);
            co_return false;
        }
        has_write_flv_header_ = true;
    } 

    int i = 0;
    send_bufs_.clear();
    for (auto & flv_tag : tags) {
        prev_tag_sizes_[i] = htonl(prev_tag_size_);
        send_bufs_.push_back(boost::asio::const_buffer((char*)&prev_tag_sizes_[i], sizeof(int32_t)));
        std::string_view tag_payload = flv_tag->get_using_data();
        send_bufs_.push_back(boost::asio::const_buffer(tag_payload.data(), tag_payload.size()));
        prev_tag_size_ = tag_payload.size();
        i++;
    }

    if (send_bufs_.size() <= 0) {
        co_return true;
    }
    
    if (!co_await http_response_->write_data(send_bufs_)) {
        close(true);
        co_return false;
    }
    co_return true;
}

void HttpFlvServerSession::close(bool close_connection) {//如果是长连接，则不关闭socket
    if (close_connection) {
        http_response_->close();
    }
    close();
}

void HttpFlvServerSession::close() {
    auto self(this->shared_from_this());
    // todo: how to record 404 error to log.
    if (closed_.test_and_set()) {
        return;
    }

    boost::asio::co_spawn(worker_->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        CORE_DEBUG("closing HttpFlvServerSession...");
        send_funcs_channel_.close();
        alive_timeout_timer_.cancel();
        co_await wg_.wait();
        
        if (flv_media_sink_) {
            auto source = flv_media_sink_->get_source();//SourceManager::get_instance().get_source(get_session_name());
            if (source) {
                source->remove_media_sink(flv_media_sink_);
            }
            
            flv_media_sink_->close();
            flv_media_sink_->on_close({});
            flv_media_sink_->on_flv_tag({});
            flv_media_sink_->set_on_source_status_changed_cb({});
            flv_media_sink_ = nullptr;
        }
        CORE_DEBUG("HttpFlvServerSession closed");
        co_return;
    }, boost::asio::detached);
}