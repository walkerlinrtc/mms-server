#include "spdlog/spdlog.h"
#include <boost/shared_ptr.hpp>
#include <memory>

#include "https_live_server.hpp"
#include "http_server_session.hpp"
#include "http_m3u8_server_session.hpp"
#include "http_ts_server_session.hpp"
#include "http_long_ts_server_session.hpp"
#include "http_flv_server_session.hpp"

#include "server/webrtc/webrtc_server.hpp"
#include "config/config.h"
#include "config/http_config.h"

using namespace mms;

HttpsLiveServer::HttpsLiveServer(ThreadWorker *w):HttpsServerBase(w) {
}

HttpsLiveServer::~HttpsLiveServer() {

}

void HttpsLiveServer::set_webrtc_server(std::shared_ptr<WebRtcServer> wrtc_server) {
    webrtc_server_ = wrtc_server;
}

std::shared_ptr<SSL_CTX> HttpsLiveServer::on_tls_ext_servername(const std::string & domain_name) {
    auto c = Config::get_instance();
    if (!c) {
        CORE_ERROR("could not find cert for:{}", domain_name);
        return nullptr;
    }
    return c->get_cert_manager().get_cert_ctx(domain_name);
}

bool HttpsLiveServer::register_route() {
    bool ret;
    ret = on_get("/:app/:stream.flv", [](std::shared_ptr<HttpServerSession> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp)->boost::asio::awaitable<void> {
        (void)session;
        auto http_flv_session = std::make_shared<HttpFlvServerSession>(req, resp);
        http_flv_session->service();
        co_return;
    });
    if (!ret) {
        spdlog::error("register on_get /:app/:stream.flv, failed");
        return false;
    }

    ret = on_get("/:app/:stream.m3u8", [](std::shared_ptr<HttpServerSession> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp)->boost::asio::awaitable<void> {
        (void)session;
        auto http_m3u8_session = std::make_shared<HttpM3u8ServerSession>(req, resp);
        http_m3u8_session->service();
        co_return;
    });
    if (!ret) {
        spdlog::error("register on_get /:app/:stream.m3u8, failed");
        return false;
    }

    ret = on_get("/:app/:stream/:id.ts", [](std::shared_ptr<HttpServerSession> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp)->boost::asio::awaitable<void> {
        (void)session;
        auto http_ts_session = std::make_shared<HttpTsServerSession>(req, resp);
        http_ts_session->service();
        co_return;
    });
    if (!ret) {
        spdlog::error("register on_get /:app/:stream/:id.ts, failed");
        return false;
    }

    ret = on_get("/:app/:stream.ts", [](std::shared_ptr<HttpServerSession> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp)->boost::asio::awaitable<void> {
        (void)session;
        auto http_long_ts_session = std::make_shared<HttpLongTsServerSession>(req, resp);
        http_long_ts_session->service();
        co_return;
    });
    if (!ret) {
        spdlog::error("register on_get /:app/:stream.ts, failed");
        return false;
    }

    ret = on_post("/:app/:stream/whip", [this](std::shared_ptr<HttpServerSession> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp)->boost::asio::awaitable<void> {
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

    ret = on_post("/:app/:stream/whep", [this](std::shared_ptr<HttpServerSession> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp)->boost::asio::awaitable<void> {
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

    auto & static_file_server_cfg = Config::get_instance()->get_https_config().get_static_file_server_config();
    if (static_file_server_cfg.is_enabled()) {
        auto path_map = static_file_server_cfg.get_path_map();
        for (auto it = path_map.begin(); it != path_map.end(); it++) {
            ret = on_static_fs(it->first, it->second);
            if (!ret) {
                return false;
            }
        }
    }
    
    // ret = on_websocket("/:app/:stream/rtc_msg", [this](std::shared_ptr<HttpServerSession> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<WsConn> ws_conn)->boost::asio::awaitable<void> {
    //     (void)req;
    //     (void)session;
    //     if (webrtc_server_) {
    //         co_await webrtc_server_->on_ws_open(ws_conn);
    //     }
    //     co_return;
    // });
    // if (!ret) {
    //     return false;
    // }

    // ret = on_get("/http3", [](std::shared_ptr<HttpServerSession<HttpsConn>> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp)->boost::asio::awaitable<void> {
    //     // 测试get接口
    //     (void)req;
    //     (void)session;
    //     resp->add_header("Content-Type", "text/plain");
    //     resp->add_header("Content-Length", "0");
    //     resp->add_header("Server", "mms");
    //     resp->add_header("Alt-Svc", "h3=\":443\"; ma=2592000,h3-29=\":443\"; ma=2592000,h3-Q050=\":443\"; ma=2592000,h3-Q046=\":443\"; ma=2592000,h3-Q043=\":443\"; ma=2592000,quic=\":443\"; ma=2592000; v=\"46,43\"");
    //     resp->add_header("Access-Control-Allow-Origin", "*");
    //     if (!(co_await resp->write_header(200, "OK"))) {
    //         resp->close();
    //         co_return;
    //     }
        
    //     resp->close();
    //     co_return;
    // });
    // if (!ret) {
    //     return false;
    // }

    return true;
}