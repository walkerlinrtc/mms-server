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

class HttpM3u8ServerSession : public StreamSession {
public:
    HttpM3u8ServerSession(std::shared_ptr<HttpRequest> http_req, std::shared_ptr<HttpResponse> http_resp);
    virtual ~HttpM3u8ServerSession();
    void service();
    void close();
private:
    std::shared_ptr<RtmpMediaSink> rtmp_media_sink_;
    std::shared_ptr<HttpRequest> http_request_;
    std::shared_ptr<HttpResponse> http_response_;
};
};