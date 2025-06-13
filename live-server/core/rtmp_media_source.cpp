#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include "rtmp_media_source.hpp"
#include "protocol/rtmp/flv/flv_tag.hpp"
#include "rtmp_media_sink.hpp"
#include "bridge/bridge_factory.hpp"
#include "bridge/media_bridge.hpp"

#include "codec/codec.hpp"
#include "codec/h264/h264_avcc.hpp"
#include "codec/h264/h264_codec.hpp"
#include "codec/aac/aac_codec.hpp"
#include "codec/mp3/mp3_codec.hpp"
#include "codec/g711/g711a_codec.hpp"
#include "codec/g711/g711u_codec.hpp"

#include "codec/hevc/hevc_codec.hpp"
#include "codec/hevc/hevc_hvcc.hpp"
#include "codec/av1/av1_codec.hpp"

#include "core/stream_session.hpp"

#include "protocol/rtmp/rtmp_define.hpp"
#include "protocol/rtmp/rtmp_message/data_message/rtmp_metadata_message.hpp"

#include "app/publish_app.h"
#include "core/error_code.hpp"
#include "spdlog/spdlog.h"
#include "base/network/bitrate_monitor.h"

using namespace mms;

RtmpMediaSource::RtmpMediaSource(ThreadWorker *worker, std::weak_ptr<StreamSession> s, std::shared_ptr<PublishApp> app) : MediaSource("rtmp", s, app, worker), av_pkts_(2048), keyframe_indexes_(200) {
    CORE_DEBUG("create RtmpMediaSource");
}

RtmpMediaSource::~RtmpMediaSource() {
    CORE_DEBUG("destroy RtmpMediaSource");
}

std::shared_ptr<Json::Value> RtmpMediaSource::to_json() {
    std::shared_ptr<Json::Value> d = std::make_shared<Json::Value>();
    Json::Value & v = *d;
    v["type"] = media_type_;
    v["domain"] = domain_name_;
    v["app"] = app_name_;
    v["stream"] = stream_name_;
    v["sinks"] = sinks_count_.load();
    v["create_at"] = create_at_;
    v["stream_time"] = time(NULL) - create_at_;
    v["client_ip"] = client_ip_;
    auto vcodec = video_codec_;
    if (vcodec) {
        v["vcodec"] = vcodec->to_json();
    }

    auto acodec = audio_codec_;
    if (acodec) {
        v["acodec"] = acodec->to_json();
    }
    
    auto session = session_.lock();
    if (session) {
        v["session"] = session->to_json();
    }
    return d;
}

bool RtmpMediaSource::add_media_sink(std::shared_ptr<MediaSink> media_sink) {
    MediaSource::add_media_sink(media_sink);
    return true;
}

std::shared_ptr<MediaBridge> RtmpMediaSource::get_or_create_bridge(const std::string & id, std::shared_ptr<PublishApp> app, const std::string & stream_name) {
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

bool RtmpMediaSource::on_audio_packet(std::shared_ptr<RtmpMessage> audio_pkt) {
    AudioTagHeader header;
    auto payload = audio_pkt->get_using_data();
    int32_t header_consumed = header.decode((uint8_t*)payload.data(), payload.size());
    if (header_consumed < 0) {
        return false;
    }

    bool sequence_header = false;
    if (!audio_ready_ && header.sound_format != AudioTagHeader::AAC) {
        audio_ready_ = true;
    } else if (header.sound_format == AudioTagHeader::AAC && header.is_seq_header()) {
        audio_header_ = audio_pkt;
        sequence_header = true;
        auto audio_config = std::make_shared<AudioSpecificConfig>();
        int32_t consumed = audio_config->parse((uint8_t*)payload.data() + header_consumed, payload.size() - header_consumed);
        if (consumed < 0) {
            CORE_ERROR("parse aac audio header failed, ret:{}", consumed);
            return false;
        }

        if (audio_codec_) {
            AACCodec *aac_codec = ((AACCodec*)audio_codec_.get());
            aac_codec->set_audio_specific_config(audio_config);
            audio_ready_ = true;
        }
        av_pkts_.clear();
    }
    latest_frame_index_ = av_pkts_.add_pkt(audio_pkt);

    if (!stream_ready_) {
        stream_ready_ = (metadata_ != nullptr) && (has_audio_?audio_ready_:true) && (has_video_?video_ready_:true);
        if (stream_ready_) {
            on_stream_ready();
        }
    }

    if (sequence_header) {
        return true;
    }

    latest_audio_timestamp_ = audio_pkt->timestamp_;
    if (latest_frame_index_ <= 300 || latest_frame_index_%10 == 0) {
        std::lock_guard<std::recursive_mutex> lck(sinks_mtx_);
        for (auto sink : sinks_) {
            auto lazy_sink = std::static_pointer_cast<LazyMediaSink>(sink);
            lazy_sink->wakeup();
        }
    }

    return true;
}

bool RtmpMediaSource::on_video_packet(std::shared_ptr<RtmpMessage> video_pkt) {
    // 解析头部
    VideoTagHeader header;
    auto payload = video_pkt->get_using_data();
    int32_t header_consumed = header.decode((uint8_t*)payload.data(), payload.size());
    if (header_consumed <= 0) {
        return false;
    }

    bool sequence_header = false;
    if (header.is_seq_header()) {
        video_ready_ = true;
        video_header_ = video_pkt;
        sequence_header = true;
        // 解析avc configuration header
        if (!video_codec_) {
            if (header.get_codec_id() == VideoTagHeader::AVC) {
                video_codec_ = std::make_shared<H264Codec>();
            } else if (header.get_codec_id() == VideoTagHeader::HEVC || header.get_codec_id() == VideoTagHeader::HEVC_FOURCC) {
                video_codec_ = std::make_shared<HevcCodec>();
            }
        }

        if (!video_codec_) {
            CORE_ERROR("rtmp:unsupport video codec:{}", (int32_t)header.get_codec_id());
            return false;
        }

        if (header.get_codec_id() == VideoTagHeader::AVC && video_codec_->get_codec_type() == CODEC_H264) {
            AVCDecoderConfigurationRecord avc_decoder_configuration_record;
            int32_t consumed = avc_decoder_configuration_record.parse((uint8_t*)payload.data() + header_consumed, payload.size() - header_consumed);
            if (consumed < 0) {
                return false;
            }
            H264Codec *h264_codec = ((H264Codec*)video_codec_.get());
            h264_codec->set_sps_pps(avc_decoder_configuration_record.get_sps(), avc_decoder_configuration_record.get_pps());
        } else if ((header.get_codec_id() == VideoTagHeader::HEVC || header.get_codec_id() == VideoTagHeader::HEVC_FOURCC) && video_codec_->get_codec_type() == CODEC_HEVC) {
            HEVCDecoderConfigurationRecord hevc_decoder_configuration_record;
            int32_t consumed = hevc_decoder_configuration_record.decode((uint8_t*)payload.data() + header_consumed, payload.size() - header_consumed);
            if (consumed < 0) {
                return false;
            }

            HevcCodec *hevc_codec = ((HevcCodec*)video_codec_.get());
            hevc_codec->set_sps_pps_vps(hevc_decoder_configuration_record.get_sps(), 
                                        hevc_decoder_configuration_record.get_pps(), 
                                        hevc_decoder_configuration_record.get_vps());
        }

        av_pkts_.clear();
    }

    latest_frame_index_ = av_pkts_.add_pkt(video_pkt);
    if (header.is_key_frame() && !header.is_seq_header()) {// 关键帧索引
        std::unique_lock<std::shared_mutex> wlock(keyframe_indexes_rw_mutex_);
        keyframe_indexes_.push_back(latest_frame_index_);
    } 

    if (!stream_ready_) {
        stream_ready_ = (metadata_ != nullptr) && (has_audio_?audio_ready_:true) && (has_video_?video_ready_:true);
        if (stream_ready_) {
            on_stream_ready();
        }
    }
    
    if (sequence_header) {
        return true;
    }

    latest_video_timestamp_ = video_pkt->timestamp_;
    if (latest_frame_index_ <= 300 || latest_frame_index_%10 == 0) {
        std::lock_guard<std::recursive_mutex> lck(sinks_mtx_);
        for (auto sink : sinks_) {
            auto lazy_sink = std::static_pointer_cast<LazyMediaSink>(sink);
            lazy_sink->wakeup();
        }
    }

    return true;
}

void RtmpMediaSource::on_stream_ready() {
    {// 创建推流
        auto s = get_session();
        if (s) {
            app_->create_push_streams(shared_from_this(), s);
        }
    }
}

bool RtmpMediaSource::on_metadata(std::shared_ptr<RtmpMessage> metadata_pkt) {
    metadata_ = std::make_shared<RtmpMetaDataMessage>();
    int ret = metadata_->decode(metadata_pkt);
    if (ret <= 0) {
        CORE_ERROR("metadata decode failed. ret={}", ret);
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
        } else {
            CORE_ERROR("rtmp:unsupport video codec:{:x}", (int32_t)video_codec_id);
            return false;
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
        } else {
            CORE_ERROR("rtmp:unsupport audio codec:{:x}", (int32_t)audio_codec_id);
            return false;
        }
    }

    return true;
}

std::vector<std::shared_ptr<RtmpMessage>> RtmpMediaSource::get_pkts(int64_t &last_pkt_index, uint32_t max_count) {
    std::vector<std::shared_ptr<RtmpMessage>> pkts;
    if (last_pkt_index == -1) {
        if (!stream_ready_) {
            CORE_WARN("stream not ready");
            return {};
        }
        // 先送头部信息
        pkts.emplace_back(metadata_->msg());
        if (has_video_) {
            if (video_header_) {
                pkts.emplace_back(video_header_);
            } 
        }

        if (has_audio_) {
            if (audio_header_) {
                pkts.emplace_back(audio_header_);
            }
        }
        
        int64_t start_idx = -1;
        if (has_video_) {
            boost::circular_buffer<uint64_t>::reverse_iterator it;
            {
                std::shared_lock<std::shared_mutex> rlock(keyframe_indexes_rw_mutex_);
                it = keyframe_indexes_.rbegin();
                while(it != keyframe_indexes_.rend()) {
                    auto pkt = av_pkts_.get_pkt(*it);
                    if (pkt) {
                        if (latest_video_timestamp_ - pkt->timestamp_ >= 2000) {
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
            int64_t index = (int64_t)av_pkts_.latest_index();
            while (index >= 0) {
                auto pkt = av_pkts_.get_pkt(index);
                if (!pkt) {
                    break;
                }

                if (latest_audio_timestamp_ - pkt->timestamp_ >= 1) {
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
            auto pkt = av_pkts_.get_pkt(start_idx);
            if (pkt) {
                pkts.emplace_back(av_pkts_.get_pkt(start_idx));
                pkt_count++;
            }
            start_idx++;
        }
        last_pkt_index = start_idx;
    } else {
        int64_t start_idx = last_pkt_index;
        uint32_t pkt_count = 0;
        while(start_idx <= latest_frame_index_ && pkt_count < max_count) {
            auto t = av_pkts_.get_pkt(start_idx);
            if (t) {
                pkts.emplace_back(av_pkts_.get_pkt(start_idx));
                pkt_count++;
            }
            start_idx++;
        }
        last_pkt_index = start_idx;
    }

    return pkts;
}