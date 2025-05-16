#pragma once
#include <memory>
#include <string>
#include <set>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <shared_mutex>

#include "base/wait_group.h"
#include "json/json.h"

namespace mms {
class MediaSink;
class MediaBridge;
class ThreadWorker;
class StreamSession;
class PublishApp;
class Recorder;
class Codec;

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
    virtual Json::Value to_json();
    void emit_event(const MediaEvent & ev);
    virtual void close();
protected:
    bool is_origin_ = true;
    std::atomic_flag closed_ = ATOMIC_FLAG_INIT;
    std::string media_type_;//rtmp,rtsp,flv,hls,ts,gb28181,webrtc,srt...
    bool stream_ready_ = false;
    std::atomic<uint32_t> sinks_count_{0};
    std::weak_ptr<StreamSession> session_;
    std::shared_ptr<PublishApp> app_;
    ThreadWorker *worker_ = nullptr;

    std::recursive_mutex sinks_mtx_;
    std::set<std::shared_ptr<MediaSink>> sinks_;//需要唤醒，才会到数据源取数据的sink
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

    std::atomic<bool> waiting_cleanup_{false};
    boost::asio::steady_timer cleanup_timer_;
    WaitGroup wg_;
};
};