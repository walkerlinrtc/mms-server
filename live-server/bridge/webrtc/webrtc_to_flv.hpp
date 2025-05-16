#pragma once
#include <memory>
#include <map>
#include <set>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/steady_timer.hpp>

#include "../media_bridge.hpp"
#include "protocol/rtmp/amf0/amf0_inc.hpp"
#include "protocol/rtmp/flv/flv_tag.hpp"
#include "protocol/rtp/rtp_h264_depacketizer.h"
#include "protocol/rtp/rtp_aac_depacketizer.h"
#include "opus/opus.h"
extern "C" {
    #include "libswresample/swresample.h"
}

#include "base/wait_group.h"

namespace mms {
class ThreadWorker;
class App;
class RtpPacket;
class RtpMediaSink;
class FlvMediaSource;
class Codec;
class RtmpMetaDataMessage;
class RtpH264NALU;
class RtpAACNALU;
class AACEncoder;
class PublishApp;

class WebRtcToFlv : public MediaBridge {
public:
    WebRtcToFlv(ThreadWorker *worker, std::shared_ptr<PublishApp>, std::weak_ptr<MediaSource> origin_source, const std::string & domain_name, const std::string & app_name, const std::string & stream_name);
    virtual ~WebRtcToFlv();

    bool init() override;
    boost::asio::awaitable<bool> on_audio_packet(std::shared_ptr<RtpPacket> audio_pkt);
    boost::asio::awaitable<bool> on_video_packet(std::shared_ptr<RtpPacket> video_pkt);
    void process_video_packet(std::shared_ptr<RtpPacket> pkt);
    void process_audio_packet(std::shared_ptr<RtpPacket> pkt);
    void close() override;
private:
    void process_h264_packet(std::shared_ptr<RtpPacket> pkt);
    void process_opus_packet(std::shared_ptr<RtpPacket> pkt);
    std::shared_ptr<FlvTag> generate_h264_flv_tag(uint32_t timestamp, std::shared_ptr<RtpH264NALU> & nalu);
    std::shared_ptr<FlvTag> generate_aac_flv_tag(uint32_t timestamp);
    bool generate_metadata();
    bool generate_video_header();
    bool generate_audio_header();
    bool generateFlvHeaders();
private:
    std::shared_ptr<RtpMediaSink> rtp_media_sink_;
    std::shared_ptr<FlvMediaSource> flv_media_source_;

    std::shared_ptr<FlvTag> metadata_pkt_;
    Amf0Object metadata_amf0_;
    std::shared_ptr<RtmpMetaDataMessage> metadata_rtmp_;
    std::shared_ptr<FlvTag> video_header_;
    std::shared_ptr<FlvTag> audio_header_;

    bool bridge_ready_ = false;

    bool has_video_ = false;
    bool has_audio_ = false;
    std::shared_ptr<Codec> video_codec_;
    std::shared_ptr<Codec> audio_codec_;
    
    boost::asio::steady_timer check_closable_timer_;
    RtpH264Depacketizer rtp_h264_depacketizer_;
    RtpAACDepacketizer rtp_aac_depacketizer_;
    std::set<uint16_t> marker_seq_no_;//marker标记为1的rtp包的seq序号

    std::unique_ptr<char[]> video_frame_cache_;
    std::unique_ptr<char[]> audio_frame_cache_;

    uint32_t first_rtp_video_ts_ = 0;
    uint32_t first_rtp_audio_ts_ = 0;

    std::shared_ptr<OpusDecoder> opus_decoder_;
    SwrContext *swr_context_ = nullptr;
    opus_int16 decoded_pcm_[8192];
    uint8_t resampled_pcm_[8192];
    uint8_t aac_data_[8192];
    int32_t aac_bytes_ = 0;
    int32_t resampled_pcm_samples_ = 0;
    std::unique_ptr<AACEncoder> aac_encoder_;
    bool header_ready_ = false;

    WaitGroup wg_;
};
};