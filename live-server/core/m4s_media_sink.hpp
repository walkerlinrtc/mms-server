#pragma once
#include <memory>
#include <functional>

#include <boost/asio/awaitable.hpp>

#include "media_sink.hpp"

namespace mms {
class Mp4Segment;
class ThreadWorker;

class M4sMediaSink : public MediaSink {
public:
    M4sMediaSink(ThreadWorker *worker);
    virtual ~M4sMediaSink();
    bool init();
    bool recv_video_init_segment(std::shared_ptr<Mp4Segment> mp4_seg);
    bool recv_audio_init_segment(std::shared_ptr<Mp4Segment> mp4_seg);
    bool recv_video_mp4_segment(std::shared_ptr<Mp4Segment> mp4_seg);
    bool recv_audio_mp4_segment(std::shared_ptr<Mp4Segment> mp4_seg);

    void set_video_init_segment_cb(const std::function<boost::asio::awaitable<bool>(std::shared_ptr<Mp4Segment> msg)> & cb);
    void set_audio_init_segment_cb(const std::function<boost::asio::awaitable<bool>(std::shared_ptr<Mp4Segment> msg)> & cb);
    void set_video_mp4_segment_cb(const std::function<boost::asio::awaitable<bool>(std::shared_ptr<Mp4Segment> msg)> & cb);
    void set_audio_mp4_segment_cb(const std::function<boost::asio::awaitable<bool>(std::shared_ptr<Mp4Segment> msg)> & cb);
    void close() override;
private:
    std::function<boost::asio::awaitable<bool>(std::shared_ptr<Mp4Segment> seg)> video_init_seg_cb_;
    std::function<boost::asio::awaitable<bool>(std::shared_ptr<Mp4Segment> seg)> audio_init_seg_cb_;
    std::function<boost::asio::awaitable<bool>(std::shared_ptr<Mp4Segment> seg)> video_seg_cb_;
    std::function<boost::asio::awaitable<bool>(std::shared_ptr<Mp4Segment> seg)> audio_seg_cb_;
};
};