#pragma once
#include <string>
#include "recorder.h"
#include "json/json.h"
#include "base/obj_tracker.hpp"

namespace mms {
class ThreadWorker;
class TsMediaSink;
class TsSegment;
class PublishApp;

class TsRecordSeg {
public:
    TsRecordSeg();
    bool load(const Json::Value & v);
public:
    int64_t create_at_;
    int64_t duration_;
    int64_t seq_no_;
    std::string file_name_;
    std::string server_;
    int64_t start_pts_;
    int64_t update_at_;
};

class TsRecorder : public Recorder, public ObjTracker<TsRecorder> {
public:
    TsRecorder(ThreadWorker *worker, std::shared_ptr<PublishApp> app, 
               std::weak_ptr<MediaSource> source, const std::string & domain_name, const std::string & app_name, 
               const std::string & stream_name);
    virtual ~TsRecorder();

    bool init() override;
    void close() override;
    std::shared_ptr<Json::Value> to_json() override;
private:
    void gen_m3u8();
    std::shared_ptr<TsMediaSink> ts_media_sink_;
    std::shared_ptr<TsSegment> prev_ts_seg_;
    int64_t seq_no_ = 1;
    int64_t record_seg_count_ = 0;
    int64_t record_duration_ = 0;
    int64_t record_start_time_ = 0;

    std::string file_dir_;
    std::vector<TsRecordSeg> ts_segs_;
    int64_t write_bytes_ = 0;
};
};