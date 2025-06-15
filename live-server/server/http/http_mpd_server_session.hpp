#pragma once

#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>

#include "core/stream_session.hpp"
#include "base/wait_group.h"

#include "base/obj_tracker.hpp"

namespace mms {
class RtmpMediaSink;
class HttpRequest;
class HttpResponse;
class ThreadWorker;

class HttpMpdServerSession : public StreamSession, public ObjTracker<HttpMpdServerSession> {
public:
    HttpMpdServerSession(std::shared_ptr<HttpRequest> http_req, std::shared_ptr<HttpResponse> http_resp);
    virtual ~HttpMpdServerSession();
    void start();
    void stop();
private:
    std::shared_ptr<RtmpMediaSink> rtmp_media_sink_;
    std::shared_ptr<HttpRequest> http_request_;
    std::shared_ptr<HttpResponse> http_response_;
};
};