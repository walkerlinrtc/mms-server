#pragma once
#include <memory>
#include <atomic>

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

namespace mms {
class ThreadWorker;

class WaitGroup {
public:
    WaitGroup(ThreadWorker *worker);
    virtual ~WaitGroup();
public:
    void add(int delta = 1);
    void done();
    boost::asio::awaitable<void> wait();
private:
    std::atomic<int> count_{0};
    ThreadWorker *worker_;
    boost::asio::steady_timer timer_;
};
};