#include "spdlog/spdlog.h"
#include <memory>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>

#include "flv_to_rtmp.hpp"

#include "base/thread/thread_worker.hpp"
#include "core/rtmp_media_source.hpp"
#include "core/flv_media_sink.hpp"

#include "protocol/rtmp/flv/flv_tag.hpp"
#include "protocol/rtmp/flv/flv_define.hpp"

#include "app/publish_app.h"
#include "config/app_config.h"

using namespace mms;

FlvToRtmp::FlvToRtmp(ThreadWorker *worker, std::shared_ptr<PublishApp> app, std::weak_ptr<MediaSource> origin_source, const std::string & domain_name, const std::string & app_name, const std::string & stream_name) : MediaBridge(worker, app, origin_source, domain_name, app_name, stream_name), check_closable_timer_(worker->get_io_context()), wg_(worker) {
    source_ = std::make_shared<RtmpMediaSource>(worker, std::weak_ptr<StreamSession>(std::shared_ptr<StreamSession>(nullptr)), publish_app_);
    rtmp_media_source_ = std::static_pointer_cast<RtmpMediaSource>(source_);
    sink_ = std::make_shared<FlvMediaSink>(worker); 
    flv_media_sink_ = std::static_pointer_cast<FlvMediaSink>(sink_);
}

FlvToRtmp::~FlvToRtmp() {
    CORE_DEBUG("destroy FlvToRtmp");
}

bool FlvToRtmp::init() {
    auto self(shared_from_this());
    wg_.add(1);
    boost::asio::co_spawn(worker_->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        boost::system::error_code ec;
        auto app_conf = publish_app_->get_conf();
        while (1) {
            check_closable_timer_.expires_from_now(std::chrono::milliseconds(app_conf->bridge_config().no_players_timeout_ms()/2));//10s检查一次
            co_await check_closable_timer_.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            if (boost::asio::error::operation_aborted == ec) {
                break;
            }

            if (rtmp_media_source_->has_no_sinks_for_time(app_conf->bridge_config().no_players_timeout_ms())) {//已经10秒没人播放了
                CORE_DEBUG("close FlvToRtmp because no players for 10s");
                break;
            }
        }
        co_return;
    }, [this, self](std::exception_ptr exp) {
        (void)exp;
        wg_.done();
        close();
    });

    flv_media_sink_->set_on_source_status_changed_cb([this, self](SourceStatus status)->boost::asio::awaitable<void> {
        rtmp_media_source_->set_status(status);
        if (status == E_SOURCE_STATUS_OK) {
            flv_media_sink_->on_flv_tag([this](const std::vector<std::shared_ptr<FlvTag>> & flv_tags)->boost::asio::awaitable<bool> {
                for (auto flv_tag : flv_tags) {
                    auto tag_type = flv_tag->get_tag_type();
                    if (tag_type == FlvTagHeader::AudioTag) {
                        if (!this->on_audio_packet(flv_tag)) {
                            co_return false;
                        }
                    } else if (tag_type == FlvTagHeader::VideoTag) {
                        if (!this->on_video_packet(flv_tag)) {
                            co_return false;
                        }
                    } else if (tag_type == FlvTagHeader::ScriptTag) {
                        if (!this->on_metadata(flv_tag)) {
                            co_return false;
                        }
                    }
                }
                
                co_return true;
            });
        }
        co_return;
    });
    return true;
}

bool FlvToRtmp::on_metadata(std::shared_ptr<FlvTag> metadata_pkt) {
    auto flv_using_data = metadata_pkt->get_using_data();
    std::shared_ptr<RtmpMessage> meta_rtmp_msg = std::make_shared<RtmpMessage>(flv_using_data.size() - FLV_TAG_HEADER_BYTES);
    meta_rtmp_msg->chunk_stream_id_ = 8;
    meta_rtmp_msg->message_stream_id_ = 0;
    meta_rtmp_msg->message_type_id_ = RTMP_MESSAGE_TYPE_AMF0_DATA;
    meta_rtmp_msg->timestamp_ = metadata_pkt->tag_header.timestamp;

    auto unuse_data = meta_rtmp_msg->get_unuse_data();
    memcpy((uint8_t*)unuse_data.data(), flv_using_data.data() + FLV_TAG_HEADER_BYTES, flv_using_data.size() - FLV_TAG_HEADER_BYTES);
    meta_rtmp_msg->inc_used_bytes(flv_using_data.size() - FLV_TAG_HEADER_BYTES);
    return rtmp_media_source_->on_metadata(meta_rtmp_msg);
} 

bool FlvToRtmp::on_audio_packet(std::shared_ptr<FlvTag> audio_pkt) {
    auto flv_using_data = audio_pkt->get_using_data();
    std::shared_ptr<RtmpMessage> audio_rtmp_msg = std::make_shared<RtmpMessage>(flv_using_data.size() - FLV_TAG_HEADER_BYTES);
    audio_rtmp_msg->chunk_stream_id_ = 8;
    audio_rtmp_msg->message_stream_id_ = 0;
    audio_rtmp_msg->message_type_id_ = RTMP_MESSAGE_TYPE_AUDIO;
    audio_rtmp_msg->timestamp_ = audio_pkt->tag_header.timestamp;

    auto unuse_data = audio_rtmp_msg->get_unuse_data();
    memcpy((uint8_t*)unuse_data.data(), flv_using_data.data() + FLV_TAG_HEADER_BYTES, flv_using_data.size() - FLV_TAG_HEADER_BYTES);
    audio_rtmp_msg->inc_used_bytes(flv_using_data.size() - FLV_TAG_HEADER_BYTES);
    return rtmp_media_source_->on_audio_packet(audio_rtmp_msg);
}

bool FlvToRtmp::on_video_packet(std::shared_ptr<FlvTag> video_pkt) {
    auto flv_using_data = video_pkt->get_using_data();
    std::shared_ptr<RtmpMessage> video_rtmp_msg = std::make_shared<RtmpMessage>(flv_using_data.size() - FLV_TAG_HEADER_BYTES);
    video_rtmp_msg->chunk_stream_id_ = 8;
    video_rtmp_msg->message_stream_id_ = 0;
    video_rtmp_msg->message_type_id_ = RTMP_MESSAGE_TYPE_VIDEO;
    video_rtmp_msg->timestamp_ = video_pkt->tag_header.timestamp;

    auto unuse_data = video_rtmp_msg->get_unuse_data();
    memcpy((void*)unuse_data.data(), flv_using_data.data()+FLV_TAG_HEADER_BYTES, flv_using_data.size() - FLV_TAG_HEADER_BYTES);
    video_rtmp_msg->inc_used_bytes(flv_using_data.size() - FLV_TAG_HEADER_BYTES);
    return rtmp_media_source_->on_video_packet(video_rtmp_msg);
}

void FlvToRtmp::close() {
    if (closed_.test_and_set(std::memory_order_acquire)) {
        return;
    }

    auto self(shared_from_this());
    boost::asio::co_spawn(worker_->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        check_closable_timer_.cancel();
        co_await wg_.wait();
        if (rtmp_media_source_) {
            rtmp_media_source_->close();
            rtmp_media_source_ = nullptr;
        }

        auto origin_source = origin_source_.lock();
        if (flv_media_sink_) {
            flv_media_sink_->on_flv_tag({});
            flv_media_sink_->close();
            if (origin_source) {
                origin_source->remove_media_sink(flv_media_sink_);
            }
            flv_media_sink_ = nullptr;
        }

        if (origin_source) {
            origin_source->remove_bridge(shared_from_this());
        }
        co_return;
    }, boost::asio::detached);
}