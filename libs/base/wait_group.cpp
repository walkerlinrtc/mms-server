#include <boost/asio/redirect_error.hpp>

#include "base/thread/thread_worker.hpp"
#include "wait_group.h"
using namespace mms;

WaitGroup::WaitGroup(ThreadWorker *worker): worker_(worker), timer_(worker->get_io_context()) {
}

WaitGroup::~WaitGroup() {

}

void WaitGroup::add(int delta) {
    count_ += delta;
}

void WaitGroup::done() {
    if (--count_ == 0) {
        timer_.cancel();
    }
}

boost::asio::awaitable<void> WaitGroup::wait() {
    if (count_ == 0) {
        co_return;
    }

    boost::system::error_code ec;
    co_await timer_.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    co_return;
}