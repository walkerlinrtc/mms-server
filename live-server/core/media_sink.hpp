#pragma once
#include <boost/asio/awaitable.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/system/error_code.hpp>

#include <memory>
#include <functional>
#include <atomic>

#include "base/wait_group.h"
#include "source_status.h"

namespace mms {
class MediaSource;
class MediaEvent;
class ThreadWorker;

class MediaSink : public std::enable_shared_from_this<MediaSink> {
    friend class MediaSource;
public:
    MediaSink(ThreadWorker *worker);
    virtual ~MediaSink();
    ThreadWorker *get_worker();

    void set_source(std::shared_ptr<MediaSource> source);
    std::shared_ptr<MediaSource> get_source();

    virtual void close();
    void on_close(const std::function<void()> & close_cb);

    void on_source_status_changed(SourceStatus status);
    void set_on_source_status_changed_cb(const std::function<boost::asio::awaitable<void>(SourceStatus)> & cb);
protected:
    std::shared_ptr<MediaSource> source_{nullptr};
    ThreadWorker *worker_;
    bool working_ = false; 

    std::function<void()> close_cb_;
    std::function<boost::asio::awaitable<void>(SourceStatus)> status_cb_;
};

class LazyMediaSink : public MediaSink {
public:
    LazyMediaSink(ThreadWorker *worker);
    virtual ~LazyMediaSink();
    virtual void wakeup();
    virtual boost::asio::awaitable<void> do_work() = 0;
    virtual void close() override;
protected:
    std::atomic<bool> closing_{false};
    WaitGroup wg_;
};

};