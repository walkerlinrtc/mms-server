#include "rtp_media_sink.hpp"
#include "codec/codec.hpp"

using namespace mms;

RtpMediaSink::RtpMediaSink(ThreadWorker *worker) : MediaSink(worker)
{

}

RtpMediaSink::~RtpMediaSink() {

}

boost::asio::awaitable<bool> RtpMediaSink::on_audio_packets(std::vector<std::shared_ptr<RtpPacket>> audio_pkts)
{
    co_return co_await audio_pkts_cb_(audio_pkts);
}

boost::asio::awaitable<bool> RtpMediaSink::on_video_packets(std::vector<std::shared_ptr<RtpPacket>> video_pkts)
{
    co_return co_await video_pkts_cb_(video_pkts);
}

void RtpMediaSink::set_video_pkts_cb(const std::function<boost::asio::awaitable<bool>(std::vector<std::shared_ptr<RtpPacket>> msg)> & cb) {
    video_pkts_cb_ = cb;
}

void RtpMediaSink::set_audio_pkts_cb(const std::function<boost::asio::awaitable<bool>(std::vector<std::shared_ptr<RtpPacket>> msg)> & cb) {
    audio_pkts_cb_ = cb;
}

bool RtpMediaSink::on_source_codec_ready(std::shared_ptr<Codec> video_codec, std::shared_ptr<Codec> audio_codec) {
    bool ret = ready_cb_(video_codec, audio_codec);
    return ret;
}

void RtpMediaSink::set_source_codec_ready_cb(const std::function<bool(std::shared_ptr<Codec> video_codec, std::shared_ptr<Codec> audio_codec)> & ready_cb) {
    ready_cb_ = ready_cb;
}

void RtpMediaSink::close() {
    ready_cb_ = {};
    video_pkts_cb_ = {};
    audio_pkts_cb_ = {};
    MediaSink::close();
}