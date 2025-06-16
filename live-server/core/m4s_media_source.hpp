/*
 * @Author: jbl19860422
 * @Date: 2023-07-02 12:15:59
 * @LastEditTime: 2023-07-02 12:16:04
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\core\ts_media_source.hpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include <set>
#include <unordered_map>
#include <string>
#include <memory>
#include <atomic>

#include <boost/circular_buffer.hpp>

#include "media_source.hpp"
#include "protocol/ts/ts_pes.hpp"
#include "base/sequence_pkt_buf.hpp"
#include "base/obj_tracker.hpp"

namespace mms {
class ThreadWorker;
class RtmpMediaSink;
class MediaBridge;
class StreamSession;
class Mp4Segment;
class PublishApp;

class M4sMediaSource : public MediaSource, public ObjTracker<M4sMediaSource> {
public:
    M4sMediaSource(ThreadWorker *worker, std::weak_ptr<StreamSession> session, std::shared_ptr<PublishApp> app);
    virtual ~M4sMediaSource();

    std::shared_ptr<Json::Value> to_json() override;
    bool init();
    bool add_media_sink(std::shared_ptr<MediaSink> media_sink);
    bool on_combined_init_segment(std::shared_ptr<Mp4Segment> mp4_seg);
    bool on_audio_init_segment(std::shared_ptr<Mp4Segment> mp4_seg);
    bool on_video_init_segment(std::shared_ptr<Mp4Segment> mp4_seg);
    bool on_audio_mp4_segment(std::shared_ptr<Mp4Segment> mp4_seg);
    bool on_video_mp4_segment(std::shared_ptr<Mp4Segment> mp4_seg);

    std::shared_ptr<MediaBridge> get_or_create_bridge(const std::string & id, std::shared_ptr<PublishApp> app, const std::string & stream_name);
    bool has_no_sinks_for_time(uint32_t milli_secs);
protected:
    std::atomic<std::shared_ptr<Mp4Segment>> combined_init_seg_;
    std::atomic<std::shared_ptr<Mp4Segment>> audio_init_seg_;
    std::atomic<std::shared_ptr<Mp4Segment>> video_init_seg_;
    std::deque<std::shared_ptr<Mp4Segment>> audio_mp4_segs_;
    std::deque<std::shared_ptr<Mp4Segment>> video_mp4_segs_;
};
};