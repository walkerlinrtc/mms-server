#pragma once
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/asio/awaitable.hpp>

#include "core/stream_session.hpp"
#include "base/wait_group.h"
#include "base/obj_tracker.hpp"

namespace mms {
class HttpRequest;
class HttpResponse;
class M4sMediaSink;
class ThreadWorker;
class Mp4Segment;

class HttpLongM4sServerSession : public StreamSession, public ObjTracker<HttpLongM4sServerSession> {
public:
    HttpLongM4sServerSession(std::shared_ptr<HttpRequest> http_req, std::shared_ptr<HttpResponse> http_resp);
    virtual ~HttpLongM4sServerSession() = default;
    void start();
    void stop();
private:
    boost::asio::awaitable<bool> send_fmp4_seg(std::shared_ptr<Mp4Segment> seg);
    boost::asio::experimental::channel<void(boost::system::error_code, std::function<boost::asio::awaitable<bool>()>)> send_funcs_channel_;
    std::shared_ptr<M4sMediaSink> mp4_media_sink_;
    std::shared_ptr<HttpRequest> http_request_;
    std::shared_ptr<HttpResponse> http_response_;
    WaitGroup wg_;
};
};