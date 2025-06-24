#pragma once
#include "core/rtp_media_sink.hpp"
#include "protocol/sdp/sdp.hpp"
#include "base/obj_tracker.hpp"

namespace mms {
class RtpPacket;
class RtspMediaSink : public RtpMediaSink, public ObjTracker<RtspMediaSink> {
public:
    RtspMediaSink(ThreadWorker *worker);
    // bool on_audio_packet(std::shared_ptr<RtpPacket> audio_pkt);
    // bool on_video_packet(std::shared_ptr<RtpPacket> video_pkt);
    Sdp & get_describe_sdp() {
        return describe_sdp_;
    }

    void set_video_pt(uint16_t v) {
        video_pt_ = v;
    }

    uint16_t get_video_pt() {
        return video_pt_;
    }

    void set_audio_pt(uint16_t v) {
        audio_pt_ = v;
    }

    uint16_t get_audio_pt() {
        return audio_pt_;
    }
private:
    Sdp describe_sdp_;
    uint16_t video_pt_ = 0;
    uint16_t audio_pt_ = 0;
};
};