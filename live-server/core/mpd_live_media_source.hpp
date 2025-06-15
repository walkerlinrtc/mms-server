#pragma once
#include <memory>
#include <string>
#include <mutex>
#include <deque>
#include <shared_mutex>

#include "core/media_source.hpp"
#include "base/obj_tracker.hpp"

namespace mms {
class Mp4Segment;
class PublishApp;
class MediaBridge;
class ThreadWorker;
class StreamSession;

class MpdLiveMediaSource : public MediaSource, public ObjTracker<MpdLiveMediaSource> {
public:
    MpdLiveMediaSource(ThreadWorker *worker, std::weak_ptr<StreamSession> session, std::shared_ptr<PublishApp> app);
    virtual ~MpdLiveMediaSource();
    // 初始化相关函数
    bool init();

    virtual bool has_no_sinks_for_time(uint32_t milli_secs);
    void update_last_access_time();

    // 数据处理相关函数
    bool on_audio_init_segment(std::shared_ptr<Mp4Segment> Mp4Segment);
    bool on_video_init_segment(std::shared_ptr<Mp4Segment> Mp4Segment);
    bool on_audio_segment(std::shared_ptr<Mp4Segment> Mp4Segment);
    bool on_video_segment(std::shared_ptr<Mp4Segment> Mp4Segment);

    // 管理及数据获取相关函数
    bool is_ready() {
        return is_ready_;
    }

    std::string get_mpd();
    void set_mpd(const std::string & v);
    std::shared_ptr<Mp4Segment> get_mp4_segment(const std::string & mp4_name);
    std::shared_ptr<MediaBridge> get_or_create_bridge(const std::string & id, std::shared_ptr<PublishApp> app, const std::string & stream_name);
protected:
    std::shared_mutex segments_mtx_;
    std::shared_ptr<Mp4Segment> audio_init_seg_;
    std::shared_ptr<Mp4Segment> video_init_seg_;
    std::deque<std::shared_ptr<Mp4Segment>> audio_segments_;
    std::deque<std::shared_ptr<Mp4Segment>> video_segments_;
    uint64_t curr_seq_no_ = 0;
    std::shared_mutex mpd_mtx_;
    std::string mpd_;
    bool is_ready_ = false;
    std::string availabilityStartTime;
private:
    void update_mpd();
};
};