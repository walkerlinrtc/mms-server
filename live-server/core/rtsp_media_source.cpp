/*
 * @Author: jbl19860422
 * @Date: 2023-12-24 20:53:55
 * @LastEditTime: 2023-12-27 21:13:11
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\core\rtsp_media_source.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include "rtsp_media_source.hpp"

#include "codec/codec.hpp"
#include "codec/h264/h264_codec.hpp"
#include "codec/hevc/hevc_codec.hpp"
#include "codec/aac/aac_codec.hpp"
#include "codec/opus/opus_codec.hpp"
#include "codec/mp3/mp3_codec.hpp"
#include "codec/g711/g711a_codec.hpp"
#include "codec/g711/g711u_codec.hpp"

#include "core/stream_session.hpp"

#include "core/rtp_media_sink.hpp"
// #include "core/media_center/media_center_manager.h"

#include "bridge/bridge_factory.hpp"
#include "bridge/media_bridge.hpp"

#include "protocol/sdp/sdp.hpp"
#include "core/error_code.hpp"
#include "spdlog/spdlog.h"
#include "app/publish_app.h"

using namespace mms;
RtspMediaSource::RtspMediaSource(ThreadWorker *worker, std::weak_ptr<StreamSession> s, std::shared_ptr<PublishApp> app) : RtpMediaSource("rtsp{rtp[es]}", s, app, worker) {

}

RtspMediaSource::~RtspMediaSource() {

}

Json::Value RtspMediaSource::to_json() {
    Json::Value v;
    v["type"] = media_type_;
    v["domain"] = domain_name_;
    v["app"] = app_name_;
    v["stream"] = stream_name_;
    v["sinks"] = sinks_count_.load();
    v["stream_time"] = 0;
    return v;
}

#define FMT_MPA 14

bool RtspMediaSource::process_announce_sdp(const std::string & sdp) {
    announce_sdp_ = std::make_shared<Sdp>();
    auto ret = announce_sdp_->parse(sdp);
    if (0 != ret)
    {
        announce_sdp_ = nullptr;
        CORE_ERROR("parse sdp failed, ret:{}", ret);
        return false;
    }

    auto &remote_medias = announce_sdp_->get_media_sdps();
    for (auto &media : remote_medias)
    {
        if (media.get_media() == "audio")
        {
            Payload *audio_payload = find_suitable_audio_payload(media);
            if (!audio_payload) {
                CORE_ERROR("could not find suitable audio payload");
                return false;
            }

            audio_pt_ = audio_payload->get_pt();
            if (!create_audio_codec_by_sdp(media, *audio_payload)) {
                CORE_ERROR("create audio codec failed");
                return false;
            }
            CORE_DEBUG("create audio codec succeed");
            has_audio_ = true;
        }
        else if (media.get_media() == "video")
        {
            Payload *video_payload = find_suitable_video_payload(media);
            if (!video_payload) {//找不到合适的视频payload
                return false;
            }
            video_pt_ = video_payload->get_pt();

            if (!create_video_codec_by_sdp(media, *video_payload)) {
                CORE_ERROR("create video codec failed");
                return false;
            }
            CORE_DEBUG("create video codec succeed");
            has_video_ = true;
        }
    }
    
    if (!stream_ready_ && video_codec_ && audio_codec_) {
        stream_ready_ = true;
        on_stream_ready();
    }

    CORE_DEBUG("RtspMediaSource video_pt:{}, audio_pt:{}", video_pt_, audio_pt_);
    return true;
}

void RtspMediaSource::on_stream_ready() {
    {// 创建推流
        auto s = get_session();
        if (s) {
            app_->create_push_streams(shared_from_this(), s);
        }
    }

    {// 给桥发信息
        std::unique_lock<std::shared_mutex> lck(bridges_mtx_);
        for (auto it = bridges_.begin(); it != bridges_.end(); it++) {
            auto media_sink = it->second->get_media_sink();
            auto s = std::static_pointer_cast<RtpMediaSink>(media_sink);
            s->on_source_codec_ready(video_codec_, audio_codec_);
        }
    }
}

Payload* RtspMediaSource::find_suitable_video_payload(MediaSdp & media_sdp) {
    Payload *match_payload = nullptr;
    auto & payloads  = media_sdp.get_payloads();
    for (auto & p : payloads) {
        std::string encoder = p.second.get_encoding_name();
        std::transform(encoder.begin(), encoder.end(), encoder.begin(), ::toupper); //将小写的都转换成大写
        if (encoder == "H264") {
            const auto &fmtps = p.second.get_fmtps();
            for (auto &pair : fmtps)
            {
                const auto &fmtp = pair.second;
                if (fmtp.get_param("packetization-mode") == "1") // h264必须时FU-A类型的才支持
                {
                    match_payload = (Payload *)&p.second;
                    break;
                }
            }
        } else if (encoder == "H265") {
            const auto &fmtps = p.second.get_fmtps();
            for (auto &pair : fmtps)
            {
                (void)pair;
                // const auto &fmtp = pair.second;
                match_payload = (Payload *)&p.second;
                break;
            }
        }

        if (match_payload) {
            break;
        }
    }
    return match_payload;
}
/*
encoder
*/
Payload* RtspMediaSource::find_suitable_audio_payload(MediaSdp & media_sdp) {
    Payload *match_payload = nullptr;
    auto & payloads  = media_sdp.get_payloads();
    for (auto & p : payloads) {
        std::string encoder = p.second.get_encoding_name();
        std::transform(encoder.begin(), encoder.end(), encoder.begin(), ::toupper); //将小写的都转换成大写
        if (encoder == "MPEG4-GENERIC") {//aac fmtp中streamtype必须是5才是audiostream，iso-14496-1 table 6中说明streamtype的作用
            match_payload = (Payload *)&p.second;
        } else if (encoder == "OPUS") {
            match_payload = (Payload *)&p.second;
        } else if (encoder == "PCMA") {
            match_payload = (Payload *)&p.second;
        }

        if (match_payload) {
            break;
        }
    }
    
    if (match_payload) {
        return match_payload;
    }
    // 容错
    if (!match_payload) {
        auto fmts = media_sdp.get_fmts();
        for (auto fmt : fmts) {
            if (fmt == FMT_MPA) {
                Payload payload;
                payload.parse("a=rtpmap:14 MPA/90000");
                media_sdp.add_payload(payload);
            }
        }
    }

    payloads  = media_sdp.get_payloads();
    for (auto & p : payloads) {
        std::string encoder = p.second.get_encoding_name();
        std::transform(encoder.begin(), encoder.end(), encoder.begin(), ::toupper); //将小写的都转换成大写
        if (encoder == "MPEG4-GENERIC") {//aac fmtp中streamtype必须是5才是audiostream，iso-14496-1 table 6中说明streamtype的作用
            match_payload = (Payload *)&p.second;
        } else if (encoder == "OPUS") {
            match_payload = (Payload *)&p.second;
        } else if (encoder == "PCMA") {
            match_payload = (Payload *)&p.second;
        } else if (encoder == "MPA") {
            match_payload = (Payload *)&p.second;
        }

        if (match_payload) {
            break;
        }
    }
    return match_payload;
}

bool RtspMediaSource::create_video_codec_by_sdp(const MediaSdp & sdp, const Payload & payload) {
    if (payload.get_encoding_name() == "H264") {
        video_codec_ = H264Codec::create_from_sdp(sdp, payload);
        if (!video_codec_) {
            return false;
        }
        video_codec_->set_data_rate(sdp.get_bw().get_value());

        auto h264_codec = (H264Codec*)video_codec_.get();
        uint32_t width, height;
        bool ret = h264_codec->get_wh(width, height);
        if (!ret) {
            return false;
        }
        CORE_DEBUG("RtspMediaSource video width:{}, height:{} from sps", width, height);
        return true;
    } else if (payload.get_encoding_name() == "H265") {
        video_codec_ = HevcCodec::create_from_sdp(sdp, payload);
        if (!video_codec_) {
            return false;
        }
        video_codec_->set_data_rate(sdp.get_bw().get_value());

        auto h265_codec = (HevcCodec*)video_codec_.get();
        uint32_t width, height;
        bool ret = h265_codec->get_wh(width, height);
        if (!ret) {
            return false;
        }
        CORE_DEBUG("RtspMediaSource video width:{}, height:{} from sps", width, height);
        return true;
    }
    
    return false;
}

bool RtspMediaSource::create_audio_codec_by_sdp(const MediaSdp & sdp, const Payload & payload) {
    std::string encoder = payload.get_encoding_name();
    std::transform(encoder.begin(), encoder.end(), encoder.begin(), ::toupper); //将小写的都转换成大写
    if (encoder == "MPEG4-GENERIC") {
        audio_codec_ = AACCodec::create_from_sdp(sdp, payload);
        if (!audio_codec_) {
            return false;
        }
        audio_codec_->set_data_rate(sdp.get_bw().get_value());
        return true;
    } else if (encoder == "OPUS") {
        audio_codec_ = OpusCodec::create_from_sdp(sdp, payload);
        if (!audio_codec_) {
            CORE_ERROR("create audio codec failed");
            return false;
        }
        return true;
    } else if (encoder == "PCMA") {
        audio_codec_ = std::make_shared<G711ACodec>();
        return true;
    } else if (encoder == "MPA") {
        audio_codec_ = std::make_shared<MP3Codec>();
        return true;
    }

    return false;
}

std::shared_ptr<MediaBridge> RtspMediaSource::get_or_create_bridge(const std::string & id, std::shared_ptr<PublishApp> app, const std::string & stream_name) {
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
    if (stream_ready_) {
        auto s = std::static_pointer_cast<RtpMediaSink>(media_sink);
        s->on_source_codec_ready(video_codec_, audio_codec_);
    }

    return bridge;
}
