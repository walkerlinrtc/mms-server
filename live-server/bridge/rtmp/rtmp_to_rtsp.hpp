#pragma once
#include <memory>
#include <list>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/awaitable.hpp>

#include "../media_bridge.hpp"
#include "protocol/rtp/rtp_packer.h"
#include "base/wait_group.h"
#include "base/obj_tracker.hpp"

namespace mms {
class ThreadWorker;
class RtmpMediaSink;
class RtspMediaSource;
class RtmpMessage;
class RtmpMetaDataMessage;
class Codec;
class PublishApp;
class Sdp;
class MediaSink;
class RtmpToRtsp : public MediaBridge, public ObjTracker<RtmpToRtsp> {
public:
    RtmpToRtsp(ThreadWorker *worker, std::shared_ptr<PublishApp>, std::weak_ptr<MediaSource> origin_source, const std::string & domain_name, const std::string & app_name, const std::string & stream_name);
    virtual ~RtmpToRtsp();
    // 主要处理函数
    bool init() override;
    bool on_metadata(std::shared_ptr<RtmpMessage> metadata_pkt);
    boost::asio::awaitable<bool> on_audio_packet(std::shared_ptr<RtmpMessage> audio_pkt);
    boost::asio::awaitable<bool> on_video_packet(std::shared_ptr<RtmpMessage> video_pkt);
    void close() override;
private:
    bool generate_sdp();
    int32_t get_nalus(uint8_t *data, int32_t len, std::list<std::string_view> & nalus);

    std::shared_ptr<RtmpMediaSink> rtmp_media_sink_;
    std::shared_ptr<RtspMediaSource> rtsp_media_source_;
    
    std::shared_ptr<RtmpMetaDataMessage> metadata_;
    std::shared_ptr<RtmpMessage> video_header_;
    std::shared_ptr<RtmpMessage> audio_header_;
    bool has_video_ = false;
    bool video_ready_ = false;
    bool has_audio_ = false;
    bool audio_ready_ = false;
    std::shared_ptr<Codec> video_codec_;
    std::shared_ptr<Codec> audio_codec_;
    int32_t nalu_length_size_ = 4;
    
    boost::asio::steady_timer check_closable_timer_;
    std::shared_ptr<Sdp> sdp_;
    uint8_t audio_pt_ = 97;
    uint8_t video_pt_ = 96;

    int64_t first_video_pts_ = 0;
    int64_t first_audio_pts_ = 0;
    bool got_video_key_frame_ = false;

    // RtpPacker video_rtp_packer_;
    std::shared_ptr<RtpPacker> video_rtp_packer_;
    RtpPacker audio_rtp_packer_;

    uint8_t audio_buf_[8192];
    int32_t audio_buf_bytes_ = 0;

    WaitGroup wg_;
private:
    boost::asio::awaitable<bool> process_h264_packet(std::shared_ptr<RtmpMessage> video_pkt);
    boost::asio::awaitable<bool> process_h265_packet(std::shared_ptr<RtmpMessage> video_pkt);
    boost::asio::awaitable<bool> process_aac_packet(std::shared_ptr<RtmpMessage> audio_pkt);
};
};