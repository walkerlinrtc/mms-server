#pragma once

#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>

#include "core/stream_session.hpp"
#include "base/wait_group.h"

namespace mms {
class RtmpMediaSink;
class HttpRequest;
class HttpResponse;
class ThreadWorker;

class HttpMpdServerSession : public StreamSession {
public:
    HttpMpdServerSession(std::shared_ptr<HttpRequest> http_req, std::shared_ptr<HttpResponse> http_resp);
    virtual ~HttpMpdServerSession();
    void service();
    void close();
private:
    std::shared_ptr<RtmpMediaSink> rtmp_media_sink_;
    std::shared_ptr<HttpRequest> http_request_;
    std::shared_ptr<HttpResponse> http_response_;
};
};