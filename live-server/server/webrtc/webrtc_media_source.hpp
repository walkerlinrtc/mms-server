#pragma once
#include "protocol/sdp/sdp.hpp"
#include "core/rtp_media_source.hpp"
#include "codec/codec.hpp"
#include "protocol/rtp/rtp_h264_depacketizer.h"

namespace mms {
class ThreadWorker;
class MediaSdp;
class DtlsCert;
class WebRtcServerSession;
class PublishApp;

class WebRtcMediaSource : public RtpMediaSource {
public:
    WebRtcMediaSource(ThreadWorker *worker, std::weak_ptr<StreamSession> session, std::shared_ptr<PublishApp> app);

    Json::Value to_json();
    std::string process_publish_sdp(const std::string & sdp);
    bool process_play_sdp(const std::string & sdp);

    uint16_t get_video_pt() const {
        return video_pt_;
    }

    uint16_t get_audio_pt() const {
        return audio_pt_;
    }

    uint32_t get_video_ssrc() {
        return video_ssrc_;
    }

    uint32_t get_audio_ssrc() {
        return audio_ssrc_;
    }

    bool create_video_codec_by_sdp(const MediaSdp & sdp, const Payload & payload);
    bool create_audio_codec_by_sdp(const MediaSdp & sdp, const Payload & payload);

    const Sdp & get_remote_sdp() const {
        return remote_sdp_;
    }

    std::shared_ptr<Sdp> get_play_offer_sdp() const {
        return play_offer_sdp_;
    }

    void set_play_offer_sdp(std::shared_ptr<Sdp> sdp);
    void set_audio_pt(uint8_t v) {
        audio_pt_ = v;
    }

    void set_video_pt(uint8_t v) {
        video_pt_ = v;
    }

    void set_audio_ssrc(uint32_t v) {
        audio_ssrc_ = v;
    }
    // boost::asio::awaitable<bool> on_video_packet(std::shared_ptr<RtpPacket> video_pkt);
    // boost::asio::awaitable<bool> on_audio_packet(std::shared_ptr<RtpPacket> audio_pkt);

    std::shared_ptr<MediaBridge> get_or_create_bridge(const std::string & id, std::shared_ptr<PublishApp> app, const std::string & stream_name);
private:
    int32_t create_local_sdp();
    void create_play_sdp();
    Payload* find_suitable_video_payload(MediaSdp & media_sdp);
    Payload* find_suitable_audio_payload(MediaSdp & media_sdp);
    
    Sdp remote_sdp_;
    Sdp local_sdp_;
    std::shared_ptr<Sdp> play_offer_sdp_;
    
    uint8_t audio_pt_ = 111;
    uint8_t video_pt_ = 127;

    uint32_t video_ssrc_ = 0;
    uint32_t audio_ssrc_ = 0;
};

};