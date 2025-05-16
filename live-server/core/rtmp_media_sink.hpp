#pragma once
#include <vector>
#include <set>
#include <atomic>

#include <boost/asio/awaitable.hpp>

#include "media_source.hpp"
#include "media_sink.hpp"

#include "rtmp_define.hpp"
#include "rtmp/rtmp_message/data_message/rtmp_metadata_message.hpp"

#include "base/thread/thread_worker.hpp"
#include "base/sequence_pkt_buf.hpp"

namespace mms {
class RtmpMediaSink : public LazyMediaSink {
public:
    RtmpMediaSink(ThreadWorker *worker);
    virtual bool init();
    virtual ~RtmpMediaSink();
    virtual bool on_audio_packet(std::shared_ptr<RtmpMessage> audio_pkt);
    virtual bool on_video_packet(std::shared_ptr<RtmpMessage> video_pkt);
    virtual bool on_metadata(std::shared_ptr<RtmpMessage> metadata_pkt);
    boost::asio::awaitable<void> do_work();
    void on_rtmp_message(const std::function<boost::asio::awaitable<bool>(const std::vector<std::shared_ptr<RtmpMessage>> & msgs)> & cb);
    void close();
protected:
    int64_t last_send_pkt_index_ = -1;
    bool has_video_; 
    bool has_audio_;
    bool stream_ready_;
    std::function<boost::asio::awaitable<bool>(const std::vector<std::shared_ptr<RtmpMessage>> & msgs)> rtmp_msg_cb_ = {};
    std::function<bool(std::shared_ptr<Codec> video_codec, std::shared_ptr<Codec> audio_codec)> ready_cb_;
};
};