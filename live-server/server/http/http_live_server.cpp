#include "log/log.h"

#include <boost/shared_ptr.hpp>
#include <memory>
#include <string>

#include "http_live_server.hpp"
#include "http_server_session.hpp"
#include "http_flv_server_session.hpp"
#include "http_ts_server_session.hpp"
#include "http_m3u8_server_session.hpp"
#include "http_mpd_server_session.hpp"
#include "http_m4s_server_session.hpp"

#include "http_long_m4s_server_session.hpp"
#include "http_long_ts_server_session.hpp"

#include "core/stream_session.hpp"
#include "core/source_manager.hpp"

#include "config/config.h"
#include "config/app_config.h"
#include "config/http_config.h"
#include "app/app_manager.h"
#include "app/app.h"

#include "server/webrtc/webrtc_server.hpp"

using namespace mms;
void HttpLiveServer::set_webrtc_server(std::shared_ptr<WebRtcServer> wrtc_server) {
    webrtc_server_ = wrtc_server;
}

bool HttpLiveServer::register_route() {
    bool ret;
    ret = on_get("/:app/:stream.flv", [](std::shared_ptr<HttpServerSession> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp)->boost::asio::awaitable<void> {
        (void)session;
        auto http_flv_session = std::make_shared<HttpFlvServerSession>(req, resp);
        http_flv_session->start(); 
        co_return;
    });
    if (!ret) {
        return false;
    }

    ret = on_options("/:app/:stream.flv", [this](std::shared_ptr<HttpServerSession> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp)->boost::asio::awaitable<void> {
        (void)session;
        (void)req;
        CORE_ERROR("get option request");
        co_await response_empty(resp);
        co_return;
    });
    if (!ret) {
        return false;
    }
    
    ret = on_get("/:app/:stream.ts", [](std::shared_ptr<HttpServerSession> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp)->boost::asio::awaitable<void> {
        (void)session;
        auto http_long_ts_session = std::make_shared<HttpLongTsServerSession>(req, resp);
        http_long_ts_session->start();
        co_return;
    });
    if (!ret) {
        return false;
    }

    ret = on_get("/:app/:stream.m3u8", [](std::shared_ptr<HttpServerSession> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp)->boost::asio::awaitable<void> {
        (void)session;
        auto http_m3u8_session = std::make_shared<HttpM3u8ServerSession>(req, resp);
        http_m3u8_session->start();
        co_return;
    });
    if (!ret) {
        return false;
    }

    ret = on_get("/:app/:stream/:seq.ts", [](std::shared_ptr<HttpServerSession> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp)->boost::asio::awaitable<void> {
        (void)session;
        auto http_ts_session = std::make_shared<HttpTsServerSession>(req, resp);
        http_ts_session->start();
        co_return;
    });
    if (!ret) {
        return false;
    }

    ret = on_get("/:app/:stream.m4s", [](std::shared_ptr<HttpServerSession> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp)->boost::asio::awaitable<void> {
        (void)session;
        auto http_long_m4s_session = std::make_shared<HttpLongM4sServerSession>(req, resp);
        http_long_m4s_session->start();
        co_return;
    });
    if (!ret) {
        return false;
    }
    
    ret = on_get("/:app/:stream.mpd", [](std::shared_ptr<HttpServerSession> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp)->boost::asio::awaitable<void> {
        (void)session;
        auto http_mpd_session = std::make_shared<HttpMpdServerSession>(req, resp);
        http_mpd_session->start();
        co_return;
    });
    if (!ret) {
        return false;
    }

    ret = on_get("/:app/:stream/:id.m4s", [](std::shared_ptr<HttpServerSession> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp)->boost::asio::awaitable<void> {
        (void)session;
        auto http_m4s_session = std::make_shared<HttpM4sServerSession>(req, resp);
        http_m4s_session->start();
        co_return;
    });
    if (!ret) {
        return false;
    }

    ret = on_post("/:app/:stream.whip", [this](std::shared_ptr<HttpServerSession> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp)->boost::asio::awaitable<void> {
        (void)session;
        if (webrtc_server_) {
            co_await webrtc_server_->on_whip(req, resp);
        } else {
            resp->close();
        }
        co_return;
    });
    if (!ret) {
        return false;
    }

    ret = on_post("/:app/:stream.whep", [this](std::shared_ptr<HttpServerSession> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp)->boost::asio::awaitable<void> {
        (void)session;
        if (webrtc_server_) {
            co_await webrtc_server_->on_whep(req, resp);
        } else {
            resp->close();
        }
        co_return;
    });
    if (!ret) {
        return false;
    }

    ret = on_patch("/:app/:stream.whep", [this](std::shared_ptr<HttpServerSession> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp)->boost::asio::awaitable<void> {
        (void)session;
        if (webrtc_server_) {
            co_await webrtc_server_->on_whep_patch(req, resp);
        } else {
            resp->close();
        }
        co_return;
    });
    if (!ret) {
        return false;
    }

    ret = on_options("/:app/:stream.whep", [this](std::shared_ptr<HttpServerSession> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp)->boost::asio::awaitable<void> {
        (void)req;
        (void)session;
        resp->add_header("Access-Control-Allow-Origin", "*");
        resp->add_header("Access-Control-Allow-Methods", "POST, PATCH, OPTIONS, GET, DELETE, HEAD");
        resp->add_header("Access-Control-Max-Age", "86400");
        std::vector<std::string> vheaders;
        auto & headers = req->get_headers();
        for (auto & p : headers) {
            vheaders.push_back(p.first);
        }
        vheaders.push_back("Content-Type");
        vheaders.push_back("If-Match");
        
        std::string allow_headers = boost::join(vheaders, ",");
        resp->add_header("Access-Control-Allow-Headers", allow_headers);
        if (!(co_await resp->write_header(204, "No Content"))) {
            resp->close();
            co_return;
        }
        resp->close();
        co_return;
    });
    if (!ret) {
        return false;
    }

    auto & static_file_server_cfg = Config::get_instance()->get_http_config().get_static_file_server_config();
    if (static_file_server_cfg.is_enabled()) {
        auto path_map = static_file_server_cfg.get_path_map();
        for (auto it = path_map.begin(); it != path_map.end(); it++) {
            ret = on_static_fs(it->first, it->second);
            if (!ret) {
                return false;
            }
        }
    }

    // ret = on_static_fs("/static/*", "/data/");
    // if (!ret) {
    //     return false;
    // }

    // ret = on_post("/api/on_publish", [](std::shared_ptr<HttpServerSession<HttpConn>> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp)->boost::asio::awaitable<void> {
    //     (void)req;
    //     (void)resp;
    //     (void)session;
    //     co_return;
    // });
    // if (!ret) {
    //     return false;
    // }

    // ret = on_get("/api/streams_count", [](std::shared_ptr<HttpServerSession<HttpConn>> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp)->boost::asio::awaitable<void> {
    //     (void)req;
    //     (void)session;
    //     resp->add_header("Content-Type", "application/json");
    //     resp->add_header("Connection", "close");
    //     Json::Value root;
    //     root["code"] = "0";
    //     root["count"] = SourceManager::get_instance().get_source_count();
    //     std::string body = root.toStyledString();
    //     resp->add_header("Content-Length", std::to_string(body.size()));
    //     if (!(co_await resp->write_header(200, "OK"))) {
    //         resp->close();
    //         co_return;
    //     }

    //     bool ret = co_await resp->write_data((const uint8_t*)(body.data()), body.size());
    //     if (!ret) {
    //         resp->close();
    //         co_return;
    //     }

    //     co_return;
    // });
    // if (!ret) {
    //     return false;
    // }

    // ret = on_get("/api/streams_info", [](std::shared_ptr<HttpServerSession<HttpConn>> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp)->boost::asio::awaitable<void> {
    //     (void)req;
    //     (void)session;
    //     resp->add_header("Content-Type", "application/json");
    //     resp->add_header("Connection", "close");
    //     Json::Value root;
    //     root["code"] = "0";
    //     auto sources = SourceManager::get_instance().get_sources();
    //     Json::Value jsources;
    //     for (auto p : sources) {
    //         jsources.append(p.second->to_json());
    //     }
    //     root["sources"] = jsources;
    //     std::string body = root.toStyledString();
    //     resp->add_header("Content-Length", std::to_string(body.size()));
    //     if (!(co_await resp->write_header(200, "OK"))) {
    //         resp->close();
    //         co_return;
    //     }

    //     bool ret = co_await resp->write_data((const uint8_t*)(body.data()), body.size());
    //     if (!ret) {
    //         resp->close();
    //         co_return;
    //     }
        
    //     co_return;
    // });
    // if (!ret) {
    //     return false;
    // }
    
    return true;
}

boost::asio::awaitable<void> HttpLiveServer::response_json(std::shared_ptr<HttpResponse> resp, int32_t code, const std::string & msg) {
    Json::Value root;
    root["code"] = code;
    root["msg"] = msg;
    std::string body = root.toStyledString();
    resp->add_header("Content-Length", std::to_string(body.size()));
    resp->add_header("Access-Control-Allow-Origin", "*");
    if (!(co_await resp->write_header(200, "OK"))) {
        resp->close();
        co_return;
    }

    bool ret = co_await resp->write_data((const uint8_t*)(body.data()), body.size());
    if (!ret) {
        resp->close();
        co_return;
    }

    resp->close();
    co_return;
}

boost::asio::awaitable<void> HttpLiveServer::response_empty(std::shared_ptr<HttpResponse> resp) {
    resp->add_header("Access-Control-Allow-Origin", "*");
    if (!(co_await resp->write_header(200, "OK"))) {
        resp->close();
        co_return;
    }

    resp->close();
    co_return;
}