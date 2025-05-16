#pragma once
#include <memory>
#include <functional>

#include <boost/asio/awaitable.hpp>

#include "media_sink.hpp"

namespace mms {
class TsSegment;
class ThreadWorker;
class PESPacket;

class TsMediaSink : public LazyMediaSink {
public:
    TsMediaSink(ThreadWorker *worker);
    virtual ~TsMediaSink();
    bool init();
    boost::asio::awaitable<void> do_work() override;
    bool recv_ts_segment(std::shared_ptr<TsSegment> ts_seg);
    void on_ts_segment(const std::function<boost::asio::awaitable<bool>(std::shared_ptr<TsSegment> msg)> & cb);
    void on_pes_pkts(const std::function<boost::asio::awaitable<bool>(const std::vector<std::shared_ptr<PESPacket>> & pkts)> & cb);
    void close() override;
private:
    int64_t last_send_pkt_index_ = -1;
    
    std::function<boost::asio::awaitable<bool>(std::shared_ptr<TsSegment> msg)> cb_;
    std::function<boost::asio::awaitable<bool>(const std::vector<std::shared_ptr<PESPacket>> & pes_pkts)> pes_pkts_cb_ = {};
};
};