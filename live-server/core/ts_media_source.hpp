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
class TsSegment;
class PublishApp;

class TsMediaSource : public MediaSource, public ObjTracker<TsMediaSource> {
public:
    TsMediaSource(ThreadWorker *worker, std::weak_ptr<StreamSession> session, std::shared_ptr<PublishApp> app);
    virtual ~TsMediaSource();

    std::shared_ptr<Json::Value> to_json() override;
    bool init();
    bool on_ts_segment(std::shared_ptr<TsSegment> ts_seg);
    bool on_pes_packet(std::shared_ptr<PESPacket> pes_packet);
    std::vector<std::shared_ptr<PESPacket>> get_pkts(int64_t &last_pkt_index, uint32_t max_count);

    std::shared_ptr<MediaBridge> get_or_create_bridge(const std::string & id, std::shared_ptr<PublishApp> app, const std::string & stream_name);
    bool has_no_sinks_for_time(uint32_t milli_secs);
protected:
    std::deque<std::shared_ptr<TsSegment>> ts_segs_;

    SequencePktBuf<PESPacket> pes_pkts_;
    std::shared_mutex keyframe_indexes_rw_mutex_;
    boost::circular_buffer<uint64_t> keyframe_indexes_;
    int64_t latest_frame_index_ = 0;

    std::shared_ptr<TsPayloadPAT> pat_;
    std::shared_ptr<TsPayloadPMT> pmt_;
    
    int32_t latest_video_timestamp_ = 0;
    int32_t latest_audio_timestamp_ = 0;
};
};