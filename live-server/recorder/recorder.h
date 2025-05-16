#pragma once
#include <memory>
#include <string>
#include <atomic>

namespace mms {
class MediaSink;
class ThreadWorker;
class PublishApp;
class MediaSource;

class Recorder : public std::enable_shared_from_this<Recorder> {
public:
    Recorder(ThreadWorker *worker, std::shared_ptr<PublishApp> app, std::weak_ptr<MediaSource> source, const std::string & domain_name, const std::string & app_name, const std::string & stream_name);
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

    virtual bool init() {
        return true;
    }

    virtual void close() {

    }
protected:
    std::atomic_flag closed_ = ATOMIC_FLAG_INIT;
    ThreadWorker *worker_;
    std::shared_ptr<PublishApp> app_;
    std::weak_ptr<MediaSource> source_;
    std::string domain_name_;
    std::string app_name_;
    std::string stream_name_;
    std::string session_name_;

    std::shared_ptr<MediaSink> sink_;
};
};