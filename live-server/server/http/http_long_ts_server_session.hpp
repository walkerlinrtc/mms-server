#pragma once
#include <atomic>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>

#include "core/stream_session.hpp"
#include "base/wait_group.h"
#include "core/source_status.h"
#include "base/obj_tracker.hpp"

namespace mms {
class HttpRequest;
class HttpResponse;
class ThreadWorker;
class TsSegment;
class TsMediaSink;
class PESPacket;

class HttpLongTsServerSession : public StreamSession, public ObjTracker<HttpLongTsServerSession> {
public:
    HttpLongTsServerSession(std::shared_ptr<HttpRequest> http_req, std::shared_ptr<HttpResponse> http_resp);
    virtual ~HttpLongTsServerSession();
    void start();
    boost::asio::awaitable<bool> send_ts_seg(std::vector<std::shared_ptr<PESPacket>> pkts);
    void start_send_coroutine();
    boost::asio::awaitable<void> process_source_status(SourceStatus status);
    void close(bool close_conn);
    void stop();
private:
    std::shared_ptr<HttpRequest> http_request_;
    std::shared_ptr<HttpResponse> http_response_;
    bool has_send_http_header_ = false;

    std::shared_ptr<TsMediaSink> ts_media_sink_;
    std::vector<boost::asio::const_buffer> send_bufs_;
    std::shared_ptr<TsSegment> prev_ts_seg_;
    boost::asio::experimental::channel<void(boost::system::error_code, std::function<boost::asio::awaitable<bool>()>)> send_funcs_channel_;
    WaitGroup wg_;
};
};