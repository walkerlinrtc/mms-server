#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include "m4s_media_sink.hpp"
#include "m4s_media_source.hpp"

#include "base/thread/thread_worker.hpp"
using namespace mms;
M4sMediaSink::M4sMediaSink(ThreadWorker *worker) : MediaSink(worker) {

}

M4sMediaSink::~M4sMediaSink() {

}

bool M4sMediaSink::init() {
    return true;
}

void M4sMediaSink::close() {
    MediaSink::close();
    return;
}

bool M4sMediaSink::recv_combined_init_segment(std::shared_ptr<Mp4Segment> mp4_seg) {
    auto self(this->shared_from_this());
    boost::asio::co_spawn(worker_->get_io_context(), [this, self, mp4_seg]()->boost::asio::awaitable<void> {
        if (!combined_init_seg_cb_) {
            co_return;
        }
        co_await combined_init_seg_cb_(mp4_seg);
        co_return;
    }, boost::asio::detached);
    return true;
}

bool M4sMediaSink::recv_video_init_segment(std::shared_ptr<Mp4Segment> mp4_seg) {
    auto self(this->shared_from_this());
    boost::asio::co_spawn(worker_->get_io_context(), [this, self, mp4_seg]()->boost::asio::awaitable<void> {
        if (!video_init_seg_cb_) {
            co_return;
        }
        co_await video_init_seg_cb_(mp4_seg);
        co_return;
    }, boost::asio::detached);
    return true;
}

bool M4sMediaSink::recv_audio_init_segment(std::shared_ptr<Mp4Segment> mp4_seg) {
    auto self(this->shared_from_this());
    boost::asio::co_spawn(worker_->get_io_context(), [this, self, mp4_seg]()->boost::asio::awaitable<void> {
        if (!audio_init_seg_cb_) {
            co_return;
        }
        co_await audio_init_seg_cb_(mp4_seg);
        co_return;
    }, boost::asio::detached);
    return true;
}

bool M4sMediaSink::recv_video_mp4_segment(std::shared_ptr<Mp4Segment> mp4_seg) {
    auto self(this->shared_from_this());
    boost::asio::co_spawn(worker_->get_io_context(), [this, self, mp4_seg]()->boost::asio::awaitable<void> {
        co_await video_seg_cb_(mp4_seg);
        co_return;
    }, boost::asio::detached);
    return true;
}

bool M4sMediaSink::recv_audio_mp4_segment(std::shared_ptr<Mp4Segment> mp4_seg) {
    auto self(this->shared_from_this());
    boost::asio::co_spawn(worker_->get_io_context(), [this, self, mp4_seg]()->boost::asio::awaitable<void> {
        co_await audio_seg_cb_(mp4_seg);
        co_return;
    }, boost::asio::detached);
    return true;
}

void M4sMediaSink::set_combined_init_segment_cb(const std::function<boost::asio::awaitable<bool>(std::shared_ptr<Mp4Segment> seg)> & cb) {
    combined_init_seg_cb_ = cb;
}

void M4sMediaSink::set_video_init_segment_cb(const std::function<boost::asio::awaitable<bool>(std::shared_ptr<Mp4Segment> seg)> & cb) {
    video_init_seg_cb_ = cb;
}

void M4sMediaSink::set_audio_init_segment_cb(const std::function<boost::asio::awaitable<bool>(std::shared_ptr<Mp4Segment> seg)> & cb) {
    audio_init_seg_cb_ = cb;
}

void M4sMediaSink::set_video_mp4_segment_cb(const std::function<boost::asio::awaitable<bool>(std::shared_ptr<Mp4Segment> seg)> & cb) {
    video_seg_cb_ = cb;
}

void M4sMediaSink::set_audio_mp4_segment_cb(const std::function<boost::asio::awaitable<bool>(std::shared_ptr<Mp4Segment> seg)> & cb) {
    audio_seg_cb_ = cb;
}
