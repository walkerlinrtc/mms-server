/*
 * @Author: jbl19860422
 * @Date: 2023-12-24 14:04:09
 * @LastEditTime: 2023-12-27 13:24:51
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\core\flv_media_source.hpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include <vector>
#include <set>
#include <shared_mutex>
#include <boost/circular_buffer.hpp>

#include "media_source.hpp"
#include "base/sequence_pkt_buf.hpp"
#include "base/obj_tracker.hpp"

namespace mms {
class ThreadWorker;
class RtmpMediaSink;
class MediaBridge;
class StreamSession;
class PublishApp;
class FlvTag;
class MediaSink;
class RtmpMetaDataMessage;

class FlvMediaSource : public MediaSource, public ObjTracker<FlvMediaSource> {
public:
    FlvMediaSource(ThreadWorker *worker, std::weak_ptr<StreamSession> session, std::shared_ptr<PublishApp> app);

    virtual ~FlvMediaSource();

    bool on_audio_packet(std::shared_ptr<FlvTag> audio_pkt);
    bool on_video_packet(std::shared_ptr<FlvTag> video_pkt);
    bool on_metadata(std::shared_ptr<FlvTag> metadata_pkt);

    std::vector<std::shared_ptr<FlvTag>> get_pkts(int64_t &last_pkt_index, uint32_t max_count);

    virtual bool add_media_sink(std::shared_ptr<MediaSink> media_sink);
    std::shared_ptr<MediaBridge> get_or_create_bridge(const std::string & id, std::shared_ptr<PublishApp> app, 
                                                      const std::string & stream_name) override;

    std::shared_ptr<Json::Value> to_json() override;
protected:
    SequencePktBuf<FlvTag> flv_tags_;
    std::shared_ptr<RtmpMetaDataMessage> metadata_;
    std::shared_ptr<FlvTag> video_header_;
    std::shared_ptr<FlvTag> audio_header_;
    std::shared_mutex keyframe_indexes_rw_mutex_;
    boost::circular_buffer<uint64_t> keyframe_indexes_;
    int64_t latest_frame_index_ = 0;
protected:
    bool paused_ = false;
    bool has_video_ = false;
    bool video_ready_ = false;
    bool has_audio_ = false;
    bool audio_ready_ = false;
    int32_t latest_video_timestamp_ = 0;
    int32_t latest_audio_timestamp_ = 0;

    std::shared_mutex waiting_ready_bridges_mtx_;
    std::unordered_map<std::string, std::shared_ptr<MediaBridge>> waiting_ready_bridges_;

    std::shared_mutex waiting_ready_sinks_mtx_;
    std::set<std::shared_ptr<MediaSink>> waiting_ready_sinks_;
    void on_stream_ready();
};
};