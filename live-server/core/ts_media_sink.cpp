#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include "ts_media_sink.hpp"
#include "core/ts_media_source.hpp"

#include "base/thread/thread_worker.hpp"
#include "log/log.h"
using namespace mms;
TsMediaSink::TsMediaSink(ThreadWorker *worker) : LazyMediaSink(worker) {

}

TsMediaSink::~TsMediaSink() {

}

void TsMediaSink::close() {
    LazyMediaSink::close();
    return;
}

bool TsMediaSink::recv_ts_segment(std::shared_ptr<TsSegment> ts_seg) {
    auto self(this->shared_from_this());
    boost::asio::co_spawn(worker_->get_io_context(), [this, self, ts_seg]()->boost::asio::awaitable<void> {
        co_await cb_(ts_seg);
        co_return;
    }, boost::asio::detached);
    return true;
}

boost::asio::awaitable<void> TsMediaSink::do_work() {
    if (source_ == nullptr || !source_->is_stream_ready()) {
        co_return;
    }
    
    auto ts_source = std::static_pointer_cast<TsMediaSource>(source_);
    std::vector<std::shared_ptr<PESPacket>> pkts;
    pkts.reserve(20);
    do {
        pkts = ts_source->get_pkts(last_send_pkt_index_, 20);
        if (!co_await pes_pkts_cb_(pkts)) {
            break;
        }
    } while (pkts.size() > 0);

    co_return;
}

void TsMediaSink::on_ts_segment(const std::function<boost::asio::awaitable<bool>(std::shared_ptr<TsSegment> msg)> & cb) {
    cb_ = cb;
}

void TsMediaSink::on_pes_pkts(const std::function<boost::asio::awaitable<bool>(const std::vector<std::shared_ptr<PESPacket>> & pkts)> & cb) {
    pes_pkts_cb_ = cb;
}