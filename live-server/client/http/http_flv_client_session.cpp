#include "http_flv_client_session.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>

#include "app/app.h"
#include "app/publish_app.h"
#include "base/sequence_pkt_buf.hpp"
#include "base/thread/thread_worker.hpp"
#include "base/utils/utils.h"
#include "config/app_config.h"
#include "config/cdn/origin_pull_config.h"
#include "core/flv_media_source.hpp"
#include "core/media_event.hpp"
#include "core/rtmp_media_source.hpp"
#include "core/source_manager.hpp"
#include "http_client.hpp"
#include "log/log.h"
#include "protocol/http/http_request.hpp"
#include "protocol/http/http_response.hpp"
#include "protocol/rtmp/flv/flv_define.hpp"
#include "protocol/rtmp/flv/flv_tag.hpp"
#include "service/dns/dns_service.hpp"

using namespace mms;

HttpFlvClientSession::HttpFlvClientSession(std::shared_ptr<PublishApp> app, ThreadWorker *worker,
                                           const std::string &domain_name, const std::string &app_name,
                                           const std::string &stream_name)
    : StreamSession(worker), flv_tags_(2048), check_closable_timer_(worker->get_io_context()), wg_(worker) {
    set_app(app);
    set_session_type("flv");
    set_session_info(domain_name, app_name, stream_name);
}

HttpFlvClientSession::~HttpFlvClientSession() {
    CORE_DEBUG("destroy HttpFlvClientSession:{}", get_session_name());
}

void HttpFlvClientSession::set_url(const std::string &url) { url_ = url; }

void HttpFlvClientSession::set_pull_config(std::shared_ptr<OriginPullConfig> pull_config) {
    pull_config_ = pull_config;
}

std::shared_ptr<FlvMediaSource> HttpFlvClientSession::get_flv_media_source() { return flv_media_source_; }

void HttpFlvClientSession::service() {
    auto self(std::static_pointer_cast<HttpFlvClientSession>(shared_from_this()));
    auto publish_app = std::static_pointer_cast<PublishApp>(app_);
    auto media_source =
        SourceManager::get_instance().get_source(get_domain_name(), get_app_name(), get_stream_name());
    if (!media_source) {
        media_source =
            std::make_shared<FlvMediaSource>(get_worker(), std::weak_ptr<StreamSession>(self), publish_app);
    }

    if (media_source->get_media_type() != "flv") {
        CORE_ERROR("source:{} is already exist and type is:{}", get_session_name(),
                   media_source->get_media_type());
        return;
    }

    flv_media_source_ = std::static_pointer_cast<FlvMediaSource>(media_source);
    flv_media_source_->set_source_info(get_domain_name(), get_app_name(), get_stream_name());
    flv_media_source_->set_session(self);
    if (!SourceManager::get_instance().add_source(get_domain_name(), get_app_name(), get_stream_name(),
                                                  flv_media_source_)) {
        CORE_ERROR("source:{} is already exist", get_session_name());
        return;
    }

    wg_.add(1);
    boost::asio::co_spawn(
        worker_->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            boost::system::error_code ec;
            while (1) {
                check_closable_timer_.expires_after(
                    std::chrono::milliseconds(pull_config_->no_players_timeout_ms() / 2));  // 10s检查一次
                co_await check_closable_timer_.async_wait(
                    boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                if (boost::asio::error::operation_aborted == ec) {
                    break;
                }

                if (!flv_media_source_) {
                    break;
                }

                if (flv_media_source_->has_no_sinks_for_time(
                        pull_config_->no_players_timeout_ms())) {  // 已经10秒没人播放了
                    CORE_DEBUG("close HttpFlvClientSession:{} because no players for {}s", get_session_name(),
                               pull_config_->no_players_timeout_ms() / 1000);
                    break;
                }
            }
            co_return;
        },
        [this, self](std::exception_ptr exp) {
            (void)exp;
            wg_.done();
            close();
        });

    wg_.add(1);
    boost::asio::co_spawn(
        get_worker()->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            // 根据url获取信息
            std::string protocol;
            std::string domain;
            uint16_t port;
            std::string path;
            std::unordered_map<std::string, std::string> params;

            if (!Utils::parse_url(url_, protocol, domain, port, path, params)) {  // todo:add log
                CORE_ERROR("HttpFlvClientSession:{} parse url:{} failed", get_session_name(), url_);
                co_return;
            }

            // 解析app名称
            std::vector<std::string> vs;
            boost::split(vs, path, boost::is_any_of("/"));
            if (vs.size() <= 2) {
                co_return;
            }

            // 获取域名ip
            auto ip = DnsService::get_instance().get_ip(domain);
            if (!ip) {
                CORE_ERROR("HttpFlvClientSession:{} could not find ip for:%s", get_session_name(),
                           domain.c_str());
                flv_media_source_->set_status(E_SOURCE_STATUS_CONN_FAIL);
                co_return;
            }
            auto &server_ip = ip.value();
            if (protocol == "http") {
                http_client_ = std::make_shared<HttpClient>(get_worker(), false);
            } else if (protocol == "https") {
                http_client_ = std::make_shared<HttpClient>(get_worker(), true);
            } else {
                close();
                co_return;
            }
            flv_media_source_->set_client_ip(server_ip);

            HttpRequest http_req;
            http_req.set_method(GET);
            http_req.set_path(path);
            http_req.set_query_params(params);
            http_req.set_version("1.1");
            http_req.add_header("Content-Length", "0");
            http_req.add_header("Accept", "*/*");
            http_req.add_header("Connection", "close");  // 表示发送完就结束，不是默认的keep-alive
            http_req.add_header("Host", domain);
            http_req.add_header("User-Agent", "mms-server");
            http_client_->set_buffer_size(1024 * 1024);
            auto resp = co_await http_client_->do_req(server_ip, port, http_req);
            if (!resp) {
                CORE_ERROR("connect to ip:{} failed", server_ip);
                flv_media_source_->set_status(E_SOURCE_STATUS_CONN_FAIL);
                co_return;
            }

            bool ret = co_await resp->recv_http_header();
            if (!ret) {
                CORE_WARN("recv http header from ip:{} failed", server_ip);
                flv_media_source_->set_status(E_SOURCE_STATUS_CONN_FAIL);
                co_return;
            }
            spdlog::info("recv http header ok, status:{}", resp->get_status_code());
            if (resp->get_status_code() == 302) {
                http_client_->close();
                http_client_.reset();
                auto location = resp->get_header("Location");
                CORE_DEBUG("HttpFlvClientSession:{} http response 302, redirect to:", get_session_name(),
                           location.c_str());
                if (!Utils::parse_url(location, protocol, domain, port, path, params)) {  // todo:add log
                    CORE_ERROR("parse http url failed, url:%s", location.c_str());
                    flv_media_source_->set_status(E_SOURCE_STATUS_CONN_FAIL);
                    co_return;
                }
                // 获取域名ip
                auto ip = DnsService::get_instance().get_ip(domain);
                if (!ip) {
                    CORE_ERROR("could not find ip for:%s", domain.c_str());
                    flv_media_source_->set_status(E_SOURCE_STATUS_CONN_FAIL);
                    co_return;
                }

                http_client_ = std::make_shared<HttpClient>(get_worker());
                auto &server_ip = ip.value();

                HttpRequest redirect_http_req;
                redirect_http_req.set_method(GET);
                redirect_http_req.set_path(path);
                redirect_http_req.set_query_params(params);
                redirect_http_req.set_version("1.1");
                redirect_http_req.add_header("Content-Length", "0");
                redirect_http_req.add_header("Accept", "*/*");
                redirect_http_req.add_header("Connection",
                                             "close");  // 表示发送完就结束，不是默认的keep-alive
                redirect_http_req.add_header("Host", domain);
                redirect_http_req.add_header("User-Agent", "mms-server");
                http_client_->set_buffer_size(1024 * 1024);
                resp = co_await http_client_->do_req(server_ip, port, redirect_http_req);
            }
            flv_media_source_->set_status(SourceStatus(resp->get_status_code()));
            if (resp->get_status_code() != 200) {  // todo: notify media event
                CORE_ERROR("http code error, code:{}", resp->get_status_code());
                co_return;
            }

            co_await cycle_pull_flv_tag(resp);
            co_return;
        },
        [this, self](std::exception_ptr exp) {
            ((void)exp);
            wg_.done();
            close();
        });
    return;
}

boost::asio::awaitable<void> HttpFlvClientSession::cycle_pull_flv_tag(std::shared_ptr<HttpResponse> resp) {
    bool flv_header_received = false;
    // todo : 这里session已经在函数hold住了，lambda没必要传值，传值会导致内存异常
    co_await resp->cycle_recv_body(
        [this, &flv_header_received](const std::string_view &recv_data) -> boost::asio::awaitable<int32_t> {
            if (!flv_header_received) {
                FlvHeader flv_header;
                int32_t consumed = flv_header.decode((uint8_t *)recv_data.data(), recv_data.size());
                if (consumed < 0) {
                    co_return -1;
                } else if (consumed == 0) {
                    co_return 0;
                }

                if (cache_tag_->get_tag_type() == FlvTagHeader::VideoTag) {
                    flv_media_source_->on_video_packet(cache_tag_);
                    cache_tag_ = nullptr;
                } else if (cache_tag_->get_tag_type() == FlvTagHeader::AudioTag) {
                    flv_media_source_->on_audio_packet(cache_tag_);
                    cache_tag_ = nullptr;
                } else {
                    flv_media_source_->on_metadata(cache_tag_);
                    cache_tag_ = nullptr;
                }
                co_return consumed + 4;
            }
        });
    co_return;
}
void HttpFlvClientSession::close() {
    // todo: how to record 404 error to log.
    if (closed_.test_and_set(std::memory_order_acquire)) {
        return;
    }

    auto self(this->shared_from_this());
    boost::asio::co_spawn(
        get_worker()->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            CORE_DEBUG("closing HttpFlvClientSession ...");
            check_closable_timer_.cancel();
            CORE_DEBUG("check_closable_timer cancel");
            if (http_client_) {
                http_client_->close();
            }
            CORE_DEBUG("close HttpFlvClientSession wait start");
            co_await wg_.wait();
            CORE_DEBUG("close HttpFlvClientSession wait start");
            if (flv_media_source_) {
                flv_media_source_->set_session(nullptr);
                auto publish_app = flv_media_source_->get_app();
                start_delayed_source_check_and_delete(publish_app->get_conf()->get_stream_resume_timeout(),
                                                      flv_media_source_);
                co_await publish_app->on_unpublish(
                    std::static_pointer_cast<StreamSession>(shared_from_this()));
            }

            if (http_client_) {
                http_client_.reset();
            }
            CORE_DEBUG("HttpFlvClientSession closed");
            co_return;
        },
        boost::asio::detached);
}