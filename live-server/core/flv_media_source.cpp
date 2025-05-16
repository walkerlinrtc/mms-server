#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include "flv_media_source.hpp"
#include "media_sink.hpp"
#include "flv_media_sink.hpp"

#include "protocol/rtmp/flv/flv_tag.hpp"

#include "core/stream_session.hpp"
#include "codec/codec.hpp"
#include "codec/aac/aac_codec.hpp"
#include "codec/mp3/mp3_codec.hpp"
#include "codec/g711/g711a_codec.hpp"
#include "codec/g711/g711u_codec.hpp"

#include "codec/h264/h264_codec.hpp"
#include "codec/hevc/hevc_codec.hpp"
#include "codec/hevc/hevc_hvcc.hpp"
#include "codec/av1/av1_codec.hpp"

#include "protocol/rtmp/rtmp_define.hpp"
#include "protocol/rtmp/rtmp_message/data_message/rtmp_metadata_message.hpp"
#include "bridge/media_bridge.hpp"
#include "bridge/bridge_factory.hpp"

#include "base/sequence_pkt_buf.hpp"
#include "base/thread/thread_worker.hpp"
#include "app/publish_app.h"

#include "spdlog/spdlog.h"

using namespace mms;

FlvMediaSource::FlvMediaSource(ThreadWorker *worker, std::weak_ptr<StreamSession> session, std::shared_ptr<PublishApp> app) : MediaSource("flv", session, app, worker), flv_tags_(2048), keyframe_indexes_(200) {
    
}

FlvMediaSource::~FlvMediaSource() {
}

Json::Value FlvMediaSource::to_json() {
    Json::Value v;
    v["type"] = media_type_;
    v["domain"] = domain_name_;
    v["app"] = app_name_;
    v["stream"] = stream_name_;
    v["sinks"] = sinks_count_.load();
    v["create_at"] = create_at_;
    v["stream_time"] = time(NULL) - create_at_;
    return v;
}

bool FlvMediaSource::on_metadata(std::shared_ptr<FlvTag> metadata_pkt) {
    metadata_ = std::make_shared<RtmpMetaDataMessage>();
    auto ret = metadata_->decode(metadata_pkt);
    if (ret <= 0) {
        spdlog::error("decode metadata failed, code:{}", ret);
        metadata_ = nullptr;
        return false;
    }
    has_video_ = metadata_->has_video();
    has_audio_ = metadata_->has_audio();

    if (has_video_) {
        auto video_codec_id = metadata_->get_video_codec_id();
        if (video_codec_id == VideoTagHeader::AVC) {
            video_codec_ = std::make_shared<H264Codec>();
        } else if (video_codec_id == VideoTagHeader::HEVC) {
            video_codec_ = std::make_shared<HevcCodec>();
        } else if (video_codec_id == VideoTagHeader::HEVC_FOURCC) {
            video_codec_ = std::make_shared<HevcCodec>();
        }
    }

    if (has_audio_) {
        auto audio_codec_id = metadata_->get_audio_codec_id();
        if (audio_codec_id == AudioTagHeader::AAC) {
            audio_codec_ = std::make_shared<AACCodec>();
        } else if (audio_codec_id == AudioTagHeader::MP3) {
            audio_codec_ = std::make_shared<MP3Codec>();
            audio_ready_ = true;
        } else if (audio_codec_id == AudioTagHeader::G711ALawPCM) {
            audio_codec_ = std::make_shared<G711ACodec>();
            audio_ready_ = true;
        } else if (audio_codec_id == AudioTagHeader::G711MuLawPCM) {
            audio_codec_ = std::make_shared<G711UCodec>();
            audio_ready_ = true;
        } 
    }
    return true;
}

bool FlvMediaSource::on_audio_packet(std::shared_ptr<FlvTag> audio_tag) {
    AUDIODATA *audio_data = (AUDIODATA *)audio_tag->tag_data.get();
    bool is_sequence_header = false;
    if (audio_data->header.sound_format == AudioTagHeader::AAC && audio_data->header.is_seq_header()) {
        audio_header_ = audio_tag;
        is_sequence_header = true;
        auto payload = audio_tag->tag_data->get_payload();
        auto audio_config = std::make_shared<AudioSpecificConfig>();
        int32_t consumed = audio_config->parse((uint8_t*)payload.data(), payload.size());
        if (consumed < 0) {
            spdlog::error("parse aac audio header failed, ret:{}", consumed);
            return false;
        }

        if (audio_codec_) {
            AACCodec *aac_codec = ((AACCodec*)audio_codec_.get());
            aac_codec->set_audio_specific_config(audio_config);
            audio_ready_ = true;
        }
        flv_tags_.clear();
    }

    latest_frame_index_ = flv_tags_.add_pkt(audio_tag);
    
    if (!stream_ready_) {
        stream_ready_ = (metadata_ != nullptr) && (has_audio_?audio_ready_:true) && (has_video_?video_ready_:true);
        if (stream_ready_) {
            on_stream_ready();
        }
    }

    if (is_sequence_header) {
        return true;
    }

    latest_audio_timestamp_ = audio_tag->tag_header.timestamp;

    if (latest_frame_index_ <= 300 || latest_frame_index_%10 == 0) {
        std::lock_guard<std::recursive_mutex> lck(sinks_mtx_);
        for (auto sink : sinks_) {
            auto lazy_sink = std::static_pointer_cast<LazyMediaSink>(sink);
            lazy_sink->wakeup();
        }
    }
    return true;
}

bool FlvMediaSource::on_video_packet(std::shared_ptr<FlvTag> video_tag) {
    VIDEODATA *video_data = (VIDEODATA *)video_tag->tag_data.get();
    bool sequence_header = false;
    if (video_data->header.is_seq_header()) {
        video_header_ = video_tag;
        sequence_header = true;
        if (video_codec_ && video_codec_->get_codec_type() == CODEC_H264) {
            AVCDecoderConfigurationRecord avc_decoder_configuration_record;
            auto payload = video_tag->tag_data->get_payload();
            int32_t consumed = avc_decoder_configuration_record.parse((uint8_t*)payload.data() , payload.size());
            if (consumed < 0) {
                spdlog::error("parse AVCDecoderConfigurationRecord failed, ret:{}", consumed);
                return false;
            }
            spdlog::debug("parse AVCDecoderConfigurationRecord succeed.");
            spdlog::debug("AVCDecoderConfigurationRecord.profile={}", (uint32_t)avc_decoder_configuration_record.avc_profile);
            ((H264Codec*)video_codec_.get())->set_sps_pps(avc_decoder_configuration_record.get_sps(), avc_decoder_configuration_record.get_pps());
            auto h264_codec = (H264Codec*)video_codec_.get();
            uint32_t w, h;
            h264_codec->get_wh(w, h);
            double fps = 0;
            h264_codec->get_fps(fps);
            spdlog::info("video width:{}, height:{}, fps:{}", w, h, fps);
        } else if (video_codec_ && video_codec_->get_codec_type() == CODEC_HEVC) {
            HEVCDecoderConfigurationRecord hevc_decoder_configuration_record;
            auto payload = video_tag->tag_data->get_payload();
            int32_t consumed = hevc_decoder_configuration_record.decode((uint8_t*)payload.data() , payload.size());
            if (consumed < 0) {
                spdlog::error("parse HEVCDecoderConfigurationRecord failed, ret:{}", consumed);
                return false;
            }
            spdlog::info("decode hevc header succeed");
        } 
        video_ready_ = true;
        flv_tags_.clear();
    }

    latest_frame_index_ = flv_tags_.add_pkt(video_tag);
    if (!stream_ready_) {
        stream_ready_ = (metadata_ != nullptr) && (has_audio_?audio_ready_:true) && (has_video_?video_ready_:true);
        if (stream_ready_) {
            on_stream_ready();
        }
    }

    if (sequence_header) {
        return true;
    }

    if (video_data->header.is_key_frame()) {// 关键帧索引
        std::unique_lock<std::shared_mutex> wlock(keyframe_indexes_rw_mutex_);
        keyframe_indexes_.push_back(latest_frame_index_);
    }  

    latest_video_timestamp_ = video_tag->tag_header.timestamp + video_data->header.composition_time;

    if (latest_frame_index_ <= 300 || latest_frame_index_%10 == 0) {
        std::lock_guard<std::recursive_mutex> lck(sinks_mtx_);
        for (auto sink : sinks_) {
            auto lazy_sink = std::static_pointer_cast<LazyMediaSink>(sink);
            lazy_sink->wakeup();
        }
    }
    return true;
}

std::vector<std::shared_ptr<FlvTag>> FlvMediaSource::get_pkts(int64_t &last_pkt_index, uint32_t max_count) {
    std::vector<std::shared_ptr<FlvTag>> pkts;
    if (last_pkt_index == -1) {
        if (!stream_ready_) {
            return {};
        }
        // 先送头部信息
        pkts.push_back(metadata_->get_flv_tag());
        if (has_video_) {
            if (video_header_) {
                pkts.push_back(video_header_);
            }
        }

        if (has_audio_) {
            if (audio_header_) {
                pkts.push_back(audio_header_);
            }
        }

        int64_t start_idx = -1;
        if (has_video_) {
            boost::circular_buffer<uint64_t>::reverse_iterator it;
            {
                std::shared_lock<std::shared_mutex> rlock(keyframe_indexes_rw_mutex_);
                it = keyframe_indexes_.rbegin();
                while(it != keyframe_indexes_.rend()) {
                    auto pkt = flv_tags_.get_pkt(*it);
                    if (pkt) {
                        VIDEODATA *video_data = (VIDEODATA *)pkt->tag_data.get();
                        if (latest_video_timestamp_ - video_data->get_dts() >= 2000) {
                            start_idx = *it;
                            break;
                        }

                        it++;
                    } else {
                        start_idx = *it;
                        break;
                    }
                }
            }
        } else {
            int64_t index = (int64_t)flv_tags_.latest_index();
            while (index >= 0) {
                auto pkt = flv_tags_.get_pkt(index);
                if (!pkt) {
                    break;
                }
                
                VIDEODATA *video_data = (VIDEODATA *)pkt->tag_data.get();
                if (latest_audio_timestamp_ - video_data->get_dts() >= 1) {
                    break;
                }
                index--;
            }
            start_idx = index;
        }

        if (start_idx < 0) {
            pkts.clear();
            return pkts;
        }

        uint32_t pkt_count = 0;
        while(start_idx <= latest_frame_index_ && pkt_count < max_count) {
            auto pkt = flv_tags_.get_pkt(start_idx);
            if (pkt) {
                pkts.emplace_back(flv_tags_.get_pkt(start_idx));
                pkt_count++;
            }
            start_idx++;
        }
        last_pkt_index = start_idx;
    } else {
        int64_t start_idx = last_pkt_index;
        
        uint32_t pkt_count = 0;
        while (start_idx <= latest_frame_index_ && pkt_count < max_count) {
            auto pkt = flv_tags_.get_pkt(start_idx);
            if (pkt) {
                pkts.emplace_back(pkt);
                pkt_count++;
            } else {
                break;
            }
            start_idx++;
        }
        last_pkt_index = start_idx;
    }
    return pkts;
}

void FlvMediaSource::on_stream_ready() {
    {// 创建推流
        auto s = get_session();
        if (s) {
            app_->create_push_streams(shared_from_this(), s);
        }
    }
}

bool FlvMediaSource::add_media_sink(std::shared_ptr<MediaSink> media_sink) {
    MediaSource::add_media_sink(media_sink);
    return true;
}

std::shared_ptr<MediaBridge> FlvMediaSource::get_or_create_bridge(const std::string & id, 
                                                                  std::shared_ptr<PublishApp> app, 
                                                                  const std::string & stream_name) {
    std::unique_lock<std::shared_mutex> lck(bridges_mtx_);
    std::shared_ptr<MediaBridge> bridge;
    auto it = bridges_.find(id);
    if (it != bridges_.end()) {
        bridge = it->second;
    } 

    if (bridge) {
        return bridge;
    }

    bridge = BridgeFactory::create_bridge(worker_, id, app, std::weak_ptr<MediaSource>(shared_from_this()), app->get_domain_name(), app->get_app_name(), stream_name);
    if (!bridge) {
        return nullptr;
    }
    bridge->init();

    auto media_sink = bridge->get_media_sink();
    media_sink->set_source(shared_from_this());
    MediaSource::add_media_sink(media_sink);

    auto media_source = bridge->get_media_source();
    media_source->set_source_info(app->get_domain_name(), app->get_app_name(), stream_name);
    
    bridges_.insert(std::pair(id, bridge));
    auto lazy_sink = std::static_pointer_cast<LazyMediaSink>(media_sink);
    lazy_sink->wakeup();
    return bridge;
}