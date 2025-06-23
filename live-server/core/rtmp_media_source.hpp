/*
 * @Author: jbl19860422
 * @Date: 2023-07-02 10:52:09
 * @LastEditTime: 2023-12-27 13:41:03
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\core\rtmp_media_source.hpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include <vector>
#include <set>
#include <shared_mutex>
#include <boost/circular_buffer.hpp>
#include <boost/asio/awaitable.hpp>

#include "media_source.hpp"
#include "base/sequence_pkt_buf.hpp"
#include "base/lockfree_seq_pkt_buf.hpp"
#include "base/obj_tracker.hpp"

namespace mms {
class ThreadWorker;
class RtmpMediaSink;
class StreamSession;
class PublishApp;
class RtmpMessage;
class MediaSink;
class RtmpMetaDataMessage;
class MediaBridge;

class RtmpMediaSource : public MediaSource, public ObjTracker<RtmpMediaSource> {
public:
    RtmpMediaSource(ThreadWorker *worker, std::weak_ptr<StreamSession> s, std::shared_ptr<PublishApp> app);
    virtual ~RtmpMediaSource();
    
    bool on_audio_packet(std::shared_ptr<RtmpMessage> audio_pkt);
    bool on_video_packet(std::shared_ptr<RtmpMessage> video_pkt);
    bool on_metadata(std::shared_ptr<RtmpMessage> metadata_pkt);

    std::vector<std::shared_ptr<RtmpMessage>> get_pkts(int64_t &last_pkt_index, uint32_t max_count);

    virtual bool add_media_sink(std::shared_ptr<MediaSink> media_sink);
    std::shared_ptr<MediaBridge> get_or_create_bridge(const std::string & id, std::shared_ptr<PublishApp> app, const std::string & stream_name);
    Json::Value to_json() override;
protected:
    SequencePktBuf<RtmpMessage> av_pkts_;
    std::shared_ptr<RtmpMetaDataMessage> metadata_;
    std::shared_ptr<RtmpMessage> video_header_;
    std::shared_ptr<RtmpMessage> audio_header_;
    std::shared_mutex keyframe_indexes_rw_mutex_;
    boost::circular_buffer<uint64_t> keyframe_indexes_;
    int64_t latest_frame_index_ = 0;
    void on_stream_ready();
protected:
    bool video_ready_ = false;
    bool audio_ready_ = false;
    int32_t latest_video_timestamp_ = 0;
    int32_t latest_audio_timestamp_ = 0;
    bool is_enhance_rtmp_ = false;
};
};