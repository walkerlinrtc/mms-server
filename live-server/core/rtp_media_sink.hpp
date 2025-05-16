#pragma once
#include <functional>
#include <memory>
#include <vector>
#include <boost/asio/awaitable.hpp>

#include "media_sink.hpp"
#include "protocol/rtp/rtp_packet.h"
namespace mms {
class Codec;

class RtpMediaSink : public MediaSink {
public:
    RtpMediaSink(ThreadWorker *worker);
    virtual ~RtpMediaSink();
    virtual bool on_source_codec_ready(std::shared_ptr<Codec> video_codec, std::shared_ptr<Codec> audio_codec);
    void set_source_codec_ready_cb(const std::function<bool(std::shared_ptr<Codec> video_codec, std::shared_ptr<Codec> audio_codec)> & ready_cb);
    virtual boost::asio::awaitable<bool> on_audio_packets(std::vector<std::shared_ptr<RtpPacket>> audio_pkts);
    virtual boost::asio::awaitable<bool> on_video_packets(std::vector<std::shared_ptr<RtpPacket>> video_pkts);
    void set_video_pkts_cb(const std::function<boost::asio::awaitable<bool>(std::vector<std::shared_ptr<RtpPacket>> msg)> & video_pkts_cb);
    void set_audio_pkts_cb(const std::function<boost::asio::awaitable<bool>(std::vector<std::shared_ptr<RtpPacket>> msg)> & audio_pkts_cb);
    void close() override;
private:
    std::function<boost::asio::awaitable<bool>(std::vector<std::shared_ptr<RtpPacket>> pkts)> video_pkts_cb_;
    std::function<boost::asio::awaitable<bool>(std::vector<std::shared_ptr<RtpPacket>> pkts)> audio_pkts_cb_;

    std::function<bool(std::shared_ptr<Codec> video_codec, std::shared_ptr<Codec> audio_codec)> ready_cb_;
};

};