#pragma once
#include <memory>
#include <string>
#include <atomic>

#include "base/obj_tracker.hpp"
#include "json/json.h"
#include "base/wait_group.h"
#include "base/thread/thread_worker.hpp"

namespace mms {
class MediaSink;
class ThreadWorker;
class PublishApp;
class MediaSource;

class Recorder : public std::enable_shared_from_this<Recorder>, public ObjTracker<Recorder> {
public:
    Recorder(const std::string & type, ThreadWorker *worker, std::shared_ptr<PublishApp> app, std::weak_ptr<MediaSource> source, const std::string & domain_name, const std::string & app_name, const std::string & stream_name);
    virtual ~Recorder();
    std::shared_ptr<MediaSink> get_media_sink() {
        return sink_;
    }

    const std::string & get_domain_name() const {
        return domain_name_;
    }

    const std::string & get_app_name() const {
        return app_name_;
    }

    const std::string & get_stream_name() const {
        return stream_name_;
    }

    std::shared_ptr<PublishApp> get_app() {
        return app_;
    }

    const std::string & type() {
        return type_;
    }

    virtual bool init() {
        return true;
    }

    virtual void close() {

    }

    virtual std::shared_ptr<Json::Value> to_json();

    template <typename R> 
    boost::asio::awaitable<std::shared_ptr<R>> sync_exec(const std::function<std::shared_ptr<R>()> & exec_func) {
        auto self(shared_from_this());
        WaitGroup wg(worker_);
        std::atomic<std::shared_ptr<R>> result = nullptr;
        wg.add(1);
        boost::asio::co_spawn(worker_->get_io_context(), [this, self, &wg, &exec_func, &result]()->boost::asio::awaitable<void> {
            result.store(exec_func(), std::memory_order_release);
            co_return;
        }, [this, self, &wg](std::exception_ptr exp) {
            (void)exp;
            wg.done();
        });

        co_await wg.wait();
        co_return result.load(std::memory_order_acquire);
    }

    boost::asio::awaitable<std::shared_ptr<Json::Value>> sync_to_json();
protected:
    std::string type_;
    std::atomic_flag closed_ = ATOMIC_FLAG_INIT;
    ThreadWorker *worker_;
    std::shared_ptr<PublishApp> app_;
    std::weak_ptr<MediaSource> source_;
    std::string domain_name_;
    std::string app_name_;
    std::string stream_name_;
    std::string session_name_;
    uint32_t create_at_ = time(NULL);

    std::shared_ptr<MediaSink> sink_;
};
};