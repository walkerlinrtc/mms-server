/*
 * @Author: jbl19860422
 * @Date: 2023-07-02 10:56:59
 * @LastEditTime: 2023-12-27 20:04:26
 * @LastEditors: jbl19860422
 * @Description:
 * @FilePath: \mms\mms\server\transcode\ts_to_hls.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved.
 */
#include "ts_to_hls.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/system/error_code.hpp>

#include "app/publish_app.h"
#include "base/thread/thread_worker.hpp"
#include "config/app_config.h"
#include "core/hls_live_media_source.hpp"
#include "core/ts_media_sink.hpp"
#include "log/log.h"


using namespace mms;
TsToHls::TsToHls(ThreadWorker *worker, std::shared_ptr<PublishApp> app,
                 std::weak_ptr<MediaSource> origin_source, const std::string &domain_name,
                 const std::string &app_name, const std::string &stream_name)
    : MediaBridge(worker, app, origin_source, domain_name, app_name, stream_name),
      check_closable_timer_(worker->get_io_context()),
      wg_(worker) {
    sink_ = std::make_shared<TsMediaSink>(worker);
    ts_media_sink_ = std::static_pointer_cast<TsMediaSink>(sink_);
    source_ = std::make_shared<HlsLiveMediaSource>(
        worker, std::weak_ptr<StreamSession>(std::shared_ptr<StreamSession>(nullptr)), publish_app_);
    hls_media_source_ = std::static_pointer_cast<HlsLiveMediaSource>(source_);
    type_ = "ts-to-hls";
}

TsToHls::~TsToHls() {}

bool TsToHls::init() {
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

                if (hls_media_source_->has_no_sinks_for_time(
                        app_conf->bridge_config().no_players_timeout_ms())) {  // 已经30秒没人播放了
                    CORE_DEBUG("close TsToHls because no players for 30s");
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

    ts_media_sink_->set_on_source_status_changed_cb(
        [this, self](SourceStatus status) -> boost::asio::awaitable<void> {
            hls_media_source_->set_status(status);
            if (status == E_SOURCE_STATUS_OK) {
                ts_media_sink_->on_ts_segment(
                    [this, self](std::shared_ptr<TsSegment> seg) -> boost::asio::awaitable<bool> {
                        hls_media_source_->on_ts_segment(seg);
                        co_return true;
                    });
            }
            co_return;
        });
    return true;
}

void TsToHls::close() {
    if (closed_.test_and_set(std::memory_order_acquire)) {
        return;
    }

    auto self(shared_from_this());
    boost::asio::co_spawn(
        worker_->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            check_closable_timer_.cancel();
            co_await wg_.wait();

            if (hls_media_source_) {
                hls_media_source_->close();
                hls_media_source_ = nullptr;
            }

            auto origin_source = origin_source_.lock();
            if (ts_media_sink_) {
                ts_media_sink_->on_ts_segment({});
                ts_media_sink_->close();
                if (origin_source) {
                    origin_source->remove_media_sink(ts_media_sink_);
                }
                ts_media_sink_ = nullptr;
            }

            if (origin_source) {
                origin_source->remove_bridge(shared_from_this());
            }
            co_return;
        },
        boost::asio::detached);
}