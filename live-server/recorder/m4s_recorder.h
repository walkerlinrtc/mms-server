#pragma once
#include <string>

#include "json/json.h"
#include "recorder.h"

#include "base/obj_tracker.hpp"

namespace mms {
class ThreadWorker;
class M4sMediaSink;
class Mp4Segment;
class PublishApp;

class M4sRecordSeg {
public:
    M4sRecordSeg();
    bool load(const Json::Value &v);

public:
    int64_t create_at_;
    int64_t duration_;
    int64_t seq_no_;
    std::string file_name_;
    std::string server_;
    int64_t start_pts_;
    int64_t update_at_;
};

class M4sRecorder : public Recorder, public ObjTracker<M4sRecorder> {
public:
    M4sRecorder(ThreadWorker *worker, std::shared_ptr<PublishApp> app, std::weak_ptr<MediaSource> source,
                 const std::string &domain_name, const std::string &app_name, const std::string &stream_name);
    virtual ~M4sRecorder();

    bool init() override;
    void close() override;
    std::shared_ptr<Json::Value> to_json() override;

    // 数据处理相关函数
    bool on_audio_init_segment(std::shared_ptr<Mp4Segment> Mp4Segment);
    bool on_video_init_segment(std::shared_ptr<Mp4Segment> Mp4Segment);
    bool on_audio_segment(std::shared_ptr<Mp4Segment> Mp4Segment);
    bool on_video_segment(std::shared_ptr<Mp4Segment> Mp4Segment);
private:
    void gen_mpd();
    std::shared_ptr<M4sMediaSink> mp4_media_sink_;
    int64_t audio_seq_no_ = 1;
    int64_t video_seq_no_ = 1;
    int64_t record_audio_seg_count_ = 0;
    int64_t record_video_seg_count_ = 0;
    int64_t record_duration_ = 0;
    int64_t record_start_time_ = 0;

    std::string file_dir_;
    std::shared_ptr<Mp4Segment> audio_init_seg_;
    std::shared_ptr<Mp4Segment> video_init_seg_;
    std::vector<M4sRecordSeg> audio_m4s_segs_;
    std::vector<M4sRecordSeg> video_m4s_segs_;
    int64_t write_bytes_ = 0;
};
};  // namespace mms