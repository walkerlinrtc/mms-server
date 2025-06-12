#pragma once
#include <memory>
#include <string>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <shared_mutex>
#include <functional>

#include "base/wait_group.h"
#include "json/json.h"
#include "source_status.h"
#include "base/thread/thread_worker.hpp"

namespace mms {
class MediaSink;
class MediaBridge;
class ThreadWorker;
class StreamSession;
class PublishApp;
class Recorder;
class Codec;
class BitrateMonitor;

class MediaEvent;

class MediaSource : public std::enable_shared_from_this<MediaSource> {
    friend class MediaSink;
public:
    MediaSource(const std::string & media_type, std::weak_ptr<StreamSession> session, std::shared_ptr<PublishApp> app, ThreadWorker *worker);

    virtual ~MediaSource();

    virtual bool add_media_sink(std::shared_ptr<MediaSink> media_sink);
    virtual bool remove_media_sink(std::shared_ptr<MediaSink> media_sink);
    
    virtual bool is_stream_ready();

    virtual std::shared_ptr<MediaBridge> get_or_create_bridge(const std::string & id, std::shared_ptr<PublishApp> app, 
                                                              const std::string & stream_name) = 0;
    virtual bool remove_bridge(std::shared_ptr<MediaBridge> bridge);
    virtual bool remove_bridge(const std::string & id);
    virtual std::shared_ptr<Recorder> get_or_create_recorder(const std::string & type, std::shared_ptr<PublishApp> app);
    
    const std::string & get_media_type() const {
        return media_type_;
    }

    void set_media_type(const std::string & v) {
        media_type_ = v;
    }
    
    std::shared_ptr<StreamSession> get_session();
    void set_session(std::shared_ptr<StreamSession> s);

    virtual bool has_no_sinks_for_time(uint32_t milli_secs);
    void set_source_info(const std::string & domain, const std::string & app_name, const std::string & stream_name);
    const std::string & get_domain_name() const {
        return domain_name_;
    }

    const std::string & get_app_name() const {
        return app_name_;
    }

    const std::string & get_stream_name() const {
        return stream_name_;
    }

    bool has_video() const {
        return has_video_;
    }

    bool has_audio() const {
        return has_audio_;
    }

    std::shared_ptr<Codec> get_video_codec() {
        return video_codec_;
    }

    std::shared_ptr<Codec> get_audio_codec() {
        return audio_codec_;
    }

    bool is_origin() const {
        return is_origin_;
    }

    void set_origin(bool v) {
        is_origin_ = v;
    }
    
    std::shared_ptr<PublishApp> get_app() {
        return app_;
    }

    SourceStatus get_status() const;
    void set_status(SourceStatus status);
    
    virtual std::shared_ptr<Json::Value> to_json();

    template <typename R> 
    boost::asio::awaitable<std::shared_ptr<R>> sync_exec(const std::function<std::shared_ptr<R>()> & exec_func) {
        auto self(shared_from_this());
        WaitGroup wg(worker_);
        std::shared_ptr<R> r;
        wg.add(1);
        boost::asio::co_spawn(worker_->get_io_context(), [this, self, &wg, &exec_func]()->boost::asio::awaitable<std::shared_ptr<R>> {
            std::shared_ptr<R> r = exec_func();
            co_return r;
        }, [this, self, &wg, &r](std::exception_ptr exp, std::shared_ptr<R> ret) {
            (void)exp;
            r = ret;
            wg.done();
        });

        co_await wg.wait();
        co_return r;
    }

    boost::asio::awaitable<std::shared_ptr<Json::Value>> sync_to_json();
    void notify_status(SourceStatus status);
    virtual void close();
protected:
    bool is_origin_ = false;
    std::atomic_flag closed_ = ATOMIC_FLAG_INIT;
    std::string media_type_;//rtmp,rtsp,flv,hls,ts,gb28181,webrtc,srt...
    bool stream_ready_ = false;
    std::atomic<uint32_t> sinks_count_{0};
    std::weak_ptr<StreamSession> session_;
    std::shared_ptr<PublishApp> app_;
    ThreadWorker *worker_ = nullptr;

    std::recursive_mutex sinks_mtx_;
    std::unordered_set<std::shared_ptr<MediaSink>> sinks_;//需要唤醒，才会到数据源取数据的sink
    std::shared_mutex bridges_mtx_;
    std::unordered_map<std::string, std::shared_ptr<MediaBridge>> bridges_;//桥接类型的sink，意味着这只是一个桥，没人的时候要拆掉，而且桥可以转换格式之类的   
    int64_t last_sinks_or_bridges_leave_time_ = -1; 
    std::shared_mutex recorder_mtx_;
    std::unordered_map<std::string, std::shared_ptr<Recorder>> recorders_;

    std::string domain_name_;
    std::string app_name_;
    std::string stream_name_;

    bool has_video_ = false;
    bool has_audio_ = false;
    std::shared_ptr<Codec> video_codec_;
    std::shared_ptr<Codec> audio_codec_;

    int64_t create_at_ = time(NULL);
    std::atomic<SourceStatus> status_{E_SOURCE_STATUS_INIT};

    std::string client_ip_;//创建该流的对端ip
};
};