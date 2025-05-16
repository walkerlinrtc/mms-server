#include <memory>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>

#include "log/log.h"
#include "rtmp_to_flv.hpp"
#include "base/thread/thread_worker.hpp"
#include "core/flv_media_source.hpp"
#include "app/publish_app.h"
#include "config/app_config.h"

using namespace mms;

RtmpToFlv::RtmpToFlv(ThreadWorker *worker, std::shared_ptr<PublishApp> app, std::weak_ptr<MediaSource> origin_source, const std::string & domain_name, const std::string & app_name, const std::string & stream_name) : MediaBridge(worker, app, origin_source, domain_name, app_name, stream_name), check_closable_timer_(worker->get_io_context()), wg_(worker) {
    source_ = std::make_shared<FlvMediaSource>(worker, std::weak_ptr<StreamSession>(std::shared_ptr<StreamSession>(nullptr)), publish_app_);
    flv_media_source_ = std::static_pointer_cast<FlvMediaSource>(source_);
    sink_ = std::make_shared<RtmpMediaSink>(worker); 
    rtmp_media_sink_ = std::static_pointer_cast<RtmpMediaSink>(sink_);
    type_ = "rtmp-to-flv";
}

RtmpToFlv::~RtmpToFlv() {
    CORE_DEBUG("destroy RtmpToFlv");
}

bool RtmpToFlv::init() {
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

            if (flv_media_source_->has_no_sinks_for_time(app_conf->bridge_config().no_players_timeout_ms())) {//已经10秒没人播放了
                CORE_DEBUG("close RtmpToFlv because no players for 10s");
                close();
                break;
            }
        }
        co_return;
    }, [this, self](std::exception_ptr exp) {
        (void)(exp);
        wg_.done();
    });

    rtmp_media_sink_->on_rtmp_message([this, self](const std::vector<std::shared_ptr<RtmpMessage>> & rtmp_msgs)->boost::asio::awaitable<bool> {
        for (auto rtmp_msg : rtmp_msgs) {
            if (rtmp_msg->get_message_type() == RTMP_MESSAGE_TYPE_AUDIO) {
                if (!this->on_audio_packet(rtmp_msg)) {
                    co_return false;
                }
            } else if (rtmp_msg->get_message_type() == RTMP_MESSAGE_TYPE_VIDEO) {
                if (!this->on_video_packet(rtmp_msg)) {
                    co_return false;
                }
            } else {
                if (!this->on_metadata(rtmp_msg)) {
                    co_return false;
                }
            }
        }
        
        co_return true;
    });
    return true;
}

bool RtmpToFlv::on_metadata(std::shared_ptr<RtmpMessage> rtmp_metadata_pkt) {
    auto metadata_pkt_using_data = rtmp_metadata_pkt->get_using_data();
    std::shared_ptr<FlvTag> script_flv_tag = std::make_shared<FlvTag>(FLV_TAG_HEADER_BYTES + metadata_pkt_using_data.size());
    script_flv_tag->tag_header.data_size = metadata_pkt_using_data.size();
    script_flv_tag->tag_header.stream_id = rtmp_metadata_pkt->message_stream_id_;
    script_flv_tag->tag_header.tag_type = FlvTagHeader::ScriptTag;
    script_flv_tag->tag_header.timestamp = rtmp_metadata_pkt->timestamp_;

    auto script_data = std::make_unique<SCRIPTDATA>();
    script_data->payload = metadata_pkt_using_data;
    script_flv_tag->tag_data = std::move(script_data);

    script_flv_tag->encode();
    return flv_media_source_->on_metadata(script_flv_tag);
} 

bool RtmpToFlv::on_audio_packet(std::shared_ptr<RtmpMessage> audio_pkt) {
    auto audio_pkt_using_data = audio_pkt->get_using_data();
    std::shared_ptr<FlvTag> audio_flv_tag = std::make_shared<FlvTag>(FLV_TAG_HEADER_BYTES + audio_pkt_using_data.size());
    
    audio_flv_tag->tag_header.data_size = audio_pkt_using_data.size();
    audio_flv_tag->tag_header.stream_id = audio_pkt->message_stream_id_;
    audio_flv_tag->tag_header.tag_type  = FlvTagHeader::AudioTag;
    audio_flv_tag->tag_header.timestamp = audio_pkt->timestamp_;

    auto audio_data = std::make_unique<AUDIODATA>();
    audio_data->payload = audio_pkt_using_data;
    audio_data->decode((const uint8_t*)audio_pkt_using_data.data(), audio_pkt_using_data.size());
    audio_flv_tag->tag_data = std::move(audio_data);

    audio_flv_tag->encode();
    return flv_media_source_->on_audio_packet(audio_flv_tag);
}

bool RtmpToFlv::on_video_packet(std::shared_ptr<RtmpMessage> video_pkt) {
    auto video_pkt_using_data = video_pkt->get_using_data();
    VideoTagHeader header;
    int32_t header_consumed = header.decode((uint8_t*)video_pkt_using_data.data(), video_pkt_using_data.size());
    if (header_consumed < 0) {
        return false;
    }

    std::shared_ptr<FlvTag> video_flv_tag = std::make_shared<FlvTag>(FLV_TAG_HEADER_BYTES + video_pkt_using_data.size());
    
    video_flv_tag->tag_header.data_size = video_pkt_using_data.size();
    video_flv_tag->tag_header.stream_id = video_pkt->message_stream_id_;
    video_flv_tag->tag_header.tag_type = FlvTagHeader::VideoTag;
    video_flv_tag->tag_header.timestamp = video_pkt->timestamp_;

    auto video_data = std::make_unique<VIDEODATA>();
    video_data->payload = video_pkt_using_data;
    video_data->decode((uint8_t*)video_pkt_using_data.data(), video_pkt_using_data.size());
    video_flv_tag->tag_data = std::move(video_data);

    video_flv_tag->encode();
    return flv_media_source_->on_video_packet(video_flv_tag);
}

void RtmpToFlv::close() {
    if (closed_.test_and_set(std::memory_order_acquire)) {
        return;
    }

    auto self(shared_from_this());
    boost::asio::co_spawn(worker_->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        check_closable_timer_.cancel();
        co_await wg_.wait();

        if (flv_media_source_) {
            flv_media_source_->close();
            flv_media_source_ = nullptr;
        }

        auto origin_source = origin_source_.lock();
        if (rtmp_media_sink_) {
            rtmp_media_sink_->on_rtmp_message({});
            rtmp_media_sink_->close();
            if (origin_source) {
                origin_source->remove_media_sink(rtmp_media_sink_);
            }
            rtmp_media_sink_ = nullptr;
        }

        if (origin_source) {
            origin_source->remove_bridge(shared_from_this());
        }
        co_return;
    }, boost::asio::detached);
}