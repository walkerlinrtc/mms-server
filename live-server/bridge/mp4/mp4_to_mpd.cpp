/*
 * @Author: jbl19860422
 * @Date: 2023-07-02 10:56:59
 * @LastEditTime: 2023-12-27 20:04:26
 * @LastEditors: jbl19860422
 * @Description:
 * @FilePath: \mms\mms\server\transcode\ts_to_hls.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved.
 */
#include "mp4_to_mpd.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/system/error_code.hpp>

#include "app/publish_app.h"
#include "base/thread/thread_worker.hpp"
#include "config/app_config.h"
#include "core/mp4_media_sink.hpp"
#include "core/mpd_live_media_source.hpp"
#include "log/log.h"

using namespace mms;

Mp4ToMpd::Mp4ToMpd(ThreadWorker *worker, std::shared_ptr<PublishApp> app,
                   std::weak_ptr<MediaSource> origin_source, const std::string &domain_name,
                   const std::string &app_name, const std::string &stream_name)
    : MediaBridge(worker, app, origin_source, domain_name, app_name, stream_name),
      check_closable_timer_(worker->get_io_context()),
      wg_(worker) {
    sink_ = std::make_shared<Mp4MediaSink>(worker);
    mp4_media_sink_ = std::static_pointer_cast<Mp4MediaSink>(sink_);
    source_ = std::make_shared<MpdLiveMediaSource>(
        worker, std::weak_ptr<StreamSession>(std::shared_ptr<StreamSession>(nullptr)), publish_app_);
    mpd_media_source_ = std::static_pointer_cast<MpdLiveMediaSource>(source_);
}

Mp4ToMpd::~Mp4ToMpd() {}

bool Mp4ToMpd::init() {
    auto self(shared_from_this());
    wg_.add(1);
    boost::asio::co_spawn(
        worker_->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            boost::system::error_code ec;
            auto app_conf = publish_app_->get_conf();
            while (1) {
                check_closable_timer_.expires_after(std::chrono::milliseconds(
                    app_conf->bridge_config().no_players_timeout_ms() / 2));  // 30s检查一次
                co_await check_closable_timer_.async_wait(
                    boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                if (boost::asio::error::operation_aborted == ec) {
                    break;
                }

                if (mpd_media_source_->has_no_sinks_for_time(
                        app_conf->bridge_config().no_players_timeout_ms())) {  // 已经30秒没人播放了
                    CORE_DEBUG("close Mp4ToMpd because no players for {}s",
                               app_conf->bridge_config().no_players_timeout_ms() / 1000);
                    break;
                }
            }
            co_return;
        },
        [this, self](std::exception_ptr exp) {
            (void)exp;
            wg_.done();
            close();
        });

    mp4_media_sink_->on_close([this, self]() { close(); });
    mp4_media_sink_->set_audio_init_segment_cb(
        [this, self](std::shared_ptr<Mp4Segment> seg) -> boost::asio::awaitable<bool> {
            mpd_media_source_->on_audio_init_segment(seg);
            co_return true;
        });

    mp4_media_sink_->set_video_init_segment_cb(
        [this, self](std::shared_ptr<Mp4Segment> seg) -> boost::asio::awaitable<bool> {
            mpd_media_source_->on_video_init_segment(seg);
            co_return true;
        });

    mp4_media_sink_->set_audio_mp4_segment_cb(
        [this, self](std::shared_ptr<Mp4Segment> seg) -> boost::asio::awaitable<bool> {
            mpd_media_source_->on_audio_segment(seg);
            co_return true;
        });

    mp4_media_sink_->set_video_mp4_segment_cb(
        [this, self](std::shared_ptr<Mp4Segment> seg) -> boost::asio::awaitable<bool> {
            mpd_media_source_->on_video_segment(seg);
            co_return true;
        });
    return true;
}

void Mp4ToMpd::close() {
    if (closed_.test_and_set(std::memory_order_acquire)) {
        return;
    }

    auto self(shared_from_this());
    boost::asio::co_spawn(
        worker_->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            check_closable_timer_.cancel();
            co_await wg_.wait();

            if (mpd_media_source_) {
                mpd_media_source_->close();
                mpd_media_source_ = nullptr;
            }

            auto origin_source = origin_source_.lock();
            if (mp4_media_sink_) {
                mp4_media_sink_->on_close({});
                mp4_media_sink_->set_audio_init_segment_cb({});
                mp4_media_sink_->set_video_init_segment_cb({});
                mp4_media_sink_->set_audio_mp4_segment_cb({});
                mp4_media_sink_->set_video_mp4_segment_cb({});
                mp4_media_sink_->close();
                if (origin_source) {
                    origin_source->remove_media_sink(mp4_media_sink_);
                }
                mp4_media_sink_ = nullptr;
            }

            if (origin_source) {
                origin_source->remove_bridge(shared_from_this());
            }
            co_return;
        },
        boost::asio::detached);
}