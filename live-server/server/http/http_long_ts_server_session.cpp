#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/buffer.hpp>

#include "http_long_ts_server_session.hpp"
#include "log/log.h"

#include "protocol/http/http_request.hpp"
#include "protocol/http/http_response.hpp"
#include "protocol/ts/ts_pes.hpp"
#include "protocol/ts/ts_segment.hpp"

#include "base/thread/thread_worker.hpp"

#include "core/ts_media_source.hpp"
#include "core/hls_live_media_source.hpp"
#include "core/ts_media_sink.hpp"
#include "core/source_manager.hpp"
// #include "core/media_center/media_center_client.h"
#include "bridge/media_bridge.hpp"
#include "app/app.h"
#include "app/play_app.h"
#include "app/publish_app.h"
#include "config/app_config.h"

// #include "system/system.h"
// #include "system/room_route.hpp"

using namespace mms;

HttpLongTsServerSession::HttpLongTsServerSession(std::shared_ptr<HttpRequest> http_req, std::shared_ptr<HttpResponse> http_resp):
                                                 StreamSession(http_resp->get_worker()), 
                                                 send_funcs_channel_(get_worker()->get_io_context()),
                                                 wg_(worker_)
{
    http_request_ = http_req;
    http_response_ = http_resp;
}

HttpLongTsServerSession::~HttpLongTsServerSession() {

}

void HttpLongTsServerSession::start() {
    auto self(std::static_pointer_cast<HttpLongTsServerSession>(this->shared_from_this()));
    boost::asio::co_spawn(worker_->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        auto domain = http_request_->get_query_param("domain");
        if (domain.empty()) {
            auto host = http_request_->get_header("Host");
            auto pos = host.find_first_of(":");
            if (pos != std::string::npos) {
                domain = host.substr(0, pos);
            } else {
                domain = host;
            }
        }

        auto & query_params = http_request_->get_query_params();
        for (auto & p : query_params) {
            set_param(p.first, p.second);
        }

        set_session_info(domain, http_request_->get_path_param("app"), http_request_->get_path_param("stream"));
        if (!find_and_set_app(domain_name_, app_name_, false)) {
            CORE_ERROR("could not find conf for domain:{}, app:{}", domain_name_, app_name_);
            http_response_->add_header("Content-Type", "video/MP2T");
            http_response_->add_header("Connection", "Close");
            co_await http_response_->write_header(404, "Not Found");
            stop();
            co_return;
        }

        auto play_app = std::static_pointer_cast<PlayApp>(get_app());
        // 1.查看是否需要播放鉴权，如果需要，那么进行鉴权并判断结果
        auto err = co_await play_app->on_play(self);
        if (err.code != 0) {
            http_response_->add_header("Content-Type", "video/MP2T");
            http_response_->add_header("Connection", "Close");
            co_await http_response_->write_header(403, "Forbidden");
            stop();
            co_return;
        }

        auto publish_app = play_app->get_publish_app();
        auto source_name = publish_app->get_domain_name() + "/" + app_name_ + "/" + http_request_->get_path_param("stream");
        // 2. 本地流级别回源配置
        auto source = SourceManager::get_instance().get_source(publish_app->get_domain_name(), get_app_name(), get_stream_name());
        auto endpoint = this->get_param("mms-endpoint");
        if (endpoint && endpoint.value().get() == "1") {
            // if (!source) {
            //     co_await MediaCenterClient::get_instance().notify_not_found(self);
            //     http_response_->add_header("Content-Type", "video/MP2T");
            //     http_response_->add_header("Connection", "Close");
            //     co_await http_response_->write_header(404, "Not Found");
            //     close();
            //     co_return;
            // }
        }

        // 4. 本地app级别回源配置
        if (!source) {
            source = co_await publish_app->find_media_source(self);
        }
        
        std::shared_ptr<TsMediaSource> ts_source;
        std::shared_ptr<MediaBridge> ts_bridge;
        if (!source) {//todo : reply 404
            http_response_->add_header("Connection", "close");
            http_response_->add_header("Content-Length", "0");
            http_response_->add_header("Access-Control-Allow-Origin", "*");
            co_await http_response_->write_header(404, "Not Found");
            stop();
            co_return;
        } else {
            if (source->get_media_type() != "ts") {
                ts_bridge = source->get_or_create_bridge(source->get_media_type() + "-ts", publish_app, stream_name_);
                if (!ts_bridge) {
                    http_response_->add_header("Connection", "close");
                    http_response_->add_header("Content-Length", "0");
                    http_response_->add_header("Access-Control-Allow-Origin", "*");
                    co_await http_response_->write_header(415, "Unsupported Media Type");
                    stop();
                    co_return;
                }

                ts_source = std::static_pointer_cast<TsMediaSource>(ts_bridge->get_media_source());
            } else {
                ts_source = std::static_pointer_cast<TsMediaSource>(source);
            }

            ts_media_sink_ = std::make_shared<TsMediaSink>(worker_);
            // 事件处理
            ts_media_sink_->set_on_source_status_changed_cb([this, self](SourceStatus status)->boost::asio::awaitable<void> {
                co_return co_await process_source_status(status);
            });
            
            ts_source->add_media_sink(ts_media_sink_);
        }

        co_return;
    }, boost::asio::detached);
}

boost::asio::awaitable<void> HttpLongTsServerSession::process_source_status(SourceStatus status) {
    auto self(shared_from_this());
    if (status == E_SOURCE_STATUS_OK) {
        if (!has_send_http_header_) {
            has_send_http_header_ = true;
            //找到源流，先发http送头部
            http_response_->add_header("Content-Type", "video/MP2T");
            http_response_->add_header("Connection", "close");
            http_response_->add_header("Access-Control-Allow-Origin", "*");
            if (!co_await http_response_->write_header(200, "Ok")) {
                close(true);
                co_return;
            }
        }
        // 发送完头部后，再开发送flv的tag
        start_send_coroutine();
        ts_media_sink_->on_pes_pkts([this](const std::vector<std::shared_ptr<PESPacket>> & pkts)->boost::asio::awaitable<bool> {
            boost::system::error_code ec;
            if (pkts.size() <= 0) {
                co_return true;
            }

            co_await send_funcs_channel_.async_send(boost::system::error_code{}, std::bind(&HttpLongTsServerSession::send_ts_seg, this, pkts), boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            if (ec) {
                co_return false;
            }
            co_return true;
        });
    } else if (status == E_SOURCE_STATUS_NOT_FOUND) {
        if (!has_send_http_header_) {
            has_send_http_header_ = true;
            http_response_->add_header("Content-Type", "video/MP2T");
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
            http_response_->add_header("Content-Type", "video/MP2T");
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
            http_response_->add_header("Content-Type", "video/MP2T");
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
            http_response_->add_header("Content-Type", "video/MP2T");
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

void HttpLongTsServerSession::start_send_coroutine() {
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

            bool ret = co_await send_func();
            if (!ret) {
                break;
            }
        }
        
        co_return;
    }, [this, self](std::exception_ptr exp) {
        (void)exp;
        wg_.done();
        stop();
    });
}

boost::asio::awaitable<bool> HttpLongTsServerSession::send_ts_seg(std::vector<std::shared_ptr<PESPacket>> pkts) {
    boost::system::error_code ec;
    if (pkts.size() <= 0) {
        co_return true;
    }

    send_bufs_.clear();
    for (auto & pes_pkt : pkts) {
        auto ts_seg = pes_pkt->ts_seg;
        auto ts_datas = ts_seg->get_ts_data();
        if (ts_seg != prev_ts_seg_) {// 对于新的切片，切片前面必定是pat和pmt
            boost::asio::const_buffer ts(ts_datas[0].data(), 188*2);
            send_bufs_.push_back(ts);
            prev_ts_seg_ = ts_seg;
        }

        auto bufs = ts_seg->get_ts_seg(pes_pkt->ts_index, pes_pkt->ts_off, pes_pkt->ts_bytes);
        send_bufs_.insert(send_bufs_.end(), bufs.begin(), bufs.end());
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

void HttpLongTsServerSession::close(bool close_conn) {
    if (close_conn) {
        http_response_->close();
    }
    stop();
}

void HttpLongTsServerSession::stop() {
    // todo: how to record 404 error to log.
    if (closed_.test_and_set()) {
        return;
    }

    auto self(shared_from_this());
    boost::asio::co_spawn(worker_->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        CORE_DEBUG("closing HttpLongTsServerSession...");
        send_funcs_channel_.close();
        co_await wg_.wait();
        if (ts_media_sink_) {
            auto source = ts_media_sink_->get_source();//SourceManager::get_instance().get_source(get_session_name());
            if (source) {
                source->remove_media_sink(ts_media_sink_);
                ts_media_sink_->close();
            }

            ts_media_sink_ = nullptr;
        }
        CORE_DEBUG("HttpLongTsServerSession closed");
        co_return;
    }, boost::asio::detached);
}