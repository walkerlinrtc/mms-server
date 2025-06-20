#include "spdlog/spdlog.h"

#include "config/config.h"
#include "codec/h264/h264_codec.hpp"
#include "codec/aac/aac_codec.hpp"
#include "core/stream_session.hpp"
#include "core/rtp_media_sink.hpp"
#include "bridge/bridge_factory.hpp"
#include "bridge/media_bridge.hpp"
#include "base/utils/utils.h"
#include "server/webrtc/dtls/dtls_cert.h"
#include "server/webrtc/webrtc_server_session.hpp"
#include "webrtc_media_source.hpp"
#include "codec/opus/opus_codec.hpp"

#include "app/publish_app.h"

using namespace mms;
WebRtcMediaSource::WebRtcMediaSource(ThreadWorker *worker, std::weak_ptr<StreamSession> session, std::shared_ptr<PublishApp> app) : RtpMediaSource("webrtc{rtp[es]}", session, app, worker) {
}

std::shared_ptr<Json::Value> WebRtcMediaSource::to_json() {
    std::shared_ptr<Json::Value> d = std::make_shared<Json::Value>();
    Json::Value & v = *d;
    v["type"] = media_type_;
    v["domain"] = domain_name_;
    v["app"] = app_name_;
    v["stream"] = stream_name_;
    v["sinks"] = sinks_count_.load();
    v["create_at"] = create_at_;
    v["stream_time"] = time(NULL) - create_at_;
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


std::string WebRtcMediaSource::process_publish_sdp(const std::string & sdp) {
    CORE_DEBUG("publish sdp:");
    CORE_DEBUG("{}", sdp);
    auto session = session_.lock();
    std::shared_ptr<WebRtcServerSession> webrtc_session = std::static_pointer_cast<WebRtcServerSession>(session);
    if (!webrtc_session) {
        return "";
    }

    auto ret = remote_sdp_.parse(sdp);
    if (0 != ret)
    {
        return "";
    }

    auto remote_ice_ufrag = remote_sdp_.get_ice_ufrag();
    if (!remote_ice_ufrag)
    {
        return "";
    }
    webrtc_session->set_remote_ice_ufrag(remote_ice_ufrag.value().getUfrag());

    auto remote_ice_pwd = remote_sdp_.get_ice_pwd();
    if (!remote_ice_pwd)
    {
        return "";
    }
    webrtc_session->set_remote_ice_pwd(remote_ice_pwd.value().getPwd());

    auto &remote_medias = remote_sdp_.get_media_sdps();
    for (auto &media : remote_medias)
    {
        if (media.get_media() == "audio")
        {
            // auto & payloads  = media.get_payloads();
            Payload *audio_payload = find_suitable_audio_payload(media);
            if (!audio_payload) {
                spdlog::error("could not find suitable audio payload");
                return "";
            }
            audio_pt_ = audio_payload->get_pt();
            if (!create_audio_codec_by_sdp(media, *audio_payload)) {
                spdlog::error("create audio codec from sdp failed");
                return "";
            }

            has_audio_ = true;
        }
        else if (media.get_media() == "video")
        {
            Payload *video_payload = find_suitable_video_payload(media);
            if (!video_payload) {//找不到合适的视频payload
                spdlog::error("could not find suitable video payload");
                return "";
            }
            video_pt_ = video_payload->get_pt();
            if (!create_video_codec_by_sdp(media, *video_payload)) {
                spdlog::error("create video codec from sdp failed");
                return "";
            }
            has_video_ = true;
        }
    }

    ret = create_local_sdp();
    if (0 != ret) {
        spdlog::error("create local sdp failed, code:{}", ret);
        return "";
    }

    create_play_sdp();

    stream_ready_ = true;
    // RtpMediaSource::on_source_codec_ready();
    spdlog::debug("WebRtcMediaSource video_pt:{}, audio_pt:{}", video_pt_, audio_pt_);
    return local_sdp_.to_string();
}

int32_t WebRtcMediaSource::create_local_sdp()
{
    auto session = session_.lock();
    std::shared_ptr<WebRtcServerSession> webrtc_session = std::static_pointer_cast<WebRtcServerSession>(session);
    if (!webrtc_session) {
        return -1;
    }

    local_sdp_.set_version(0);
    local_sdp_.set_origin({"-", Utils::get_rand64(), 1, "IN", "IP4", "127.0.0.1"}); // o=- get_rand64 1 IN IP4 127.0.0.1
    local_sdp_.set_session_name(webrtc_session->get_session_name());                                  //
    local_sdp_.set_time({0, 0});                                                // t=0 0
    local_sdp_.set_tool({"mms"});
    local_sdp_.set_bundle({"video", "audio", "data"});
    local_sdp_.add_attr("ice-lite");
    local_sdp_.add_attr("msid-semantic: WMS " + webrtc_session->get_session_name());
    auto &remote_medias = remote_sdp_.get_media_sdps();
    if (remote_medias.size() > 1)
    {
        BundleAttr bundle;
        for (auto &media : remote_medias)
        {
            if (media.get_media() == "audio")
            {
                bundle.add_mid(media.get_mid_attr().value().get_mid());
            }
            else if (media.get_media() == "video")
            {
                bundle.add_mid(media.get_mid_attr().value().get_mid());
            }
        }
        local_sdp_.set_bundle(bundle);
    }

    for (auto &media : remote_medias)
    {
        if (media.get_media() == "audio")
        {
            MediaSdp audio_sdp;
            audio_sdp.set_media("audio");
            audio_sdp.set_port(9);
            audio_sdp.set_port_count(1);
            audio_sdp.set_proto("UDP/TLS/RTP/SAVPF");
            audio_sdp.add_fmt(audio_pt_);
            audio_sdp.set_connection_info({"IN", "IP4", "0.0.0.0"});
            audio_sdp.set_ice_ufrag(IceUfrag(webrtc_session->get_local_ice_ufrag()));
            audio_sdp.set_ice_pwd(IcePwd(webrtc_session->get_local_ice_pwd()));
            audio_sdp.set_dir(media.get_reverse_dir());
            audio_sdp.set_setup(SetupAttr(ROLE_PASSIVE));
            audio_sdp.set_mid_attr(media.get_mid_attr().value());
            audio_sdp.set_rtcp_mux(RtcpMux());
            audio_sdp.add_candidate(Candidate("fund_common", 1, "UDP", 2130706431, webrtc_session->get_local_ip(), webrtc_session->get_udp_port(), Candidate::CAND_TYPE_HOST, "", 0, {{"generation", "0"}}));
            if (media.get_ssrc_group())
            {
                audio_sdp.set_ssrc_group(media.get_ssrc_group().value());
            }

            for (auto &p : media.get_ssrcs())
            {
                audio_sdp.add_ssrc(Ssrc(p.second.get_id(), webrtc_session->get_session_name(), webrtc_session->get_session_name(), webrtc_session->get_session_name() + "_audio"));
                audio_ssrc_ = p.second.get_id();
            }

            audio_sdp.set_finger_print(FingerPrint("sha-1", webrtc_session->get_dtls_cert()->get_finger_print()));
            auto remote_audio_payload = media.search_payload("OPUS");
            if (!remote_audio_payload.has_value())
            {
                return -12;
            }

            auto &rap = remote_audio_payload.value();
            Payload audio_payload(audio_pt_, rap.get_encoding_name(), rap.get_clock_rate(), rap.get_encoding_params());
            audio_payload.add_rtcp_fb(RtcpFb(audio_pt_, "nack"));
            audio_payload.add_rtcp_fb(RtcpFb(audio_pt_, "nack", "pli"));
            audio_sdp.add_payload(audio_payload);

            local_sdp_.add_media_sdp(audio_sdp);
        }
        else if (media.get_media() == "video")
        {
            MediaSdp video_sdp;
            video_sdp.set_media("video");
            video_sdp.set_port(9);
            video_sdp.set_port_count(1);
            video_sdp.set_proto("UDP/TLS/RTP/SAVPF");
            video_sdp.set_connection_info({"IN", "IP4", "0.0.0.0"});
            video_sdp.set_ice_ufrag(IceUfrag(webrtc_session->get_local_ice_ufrag()));
            video_sdp.set_ice_pwd(IcePwd(webrtc_session->get_local_ice_pwd()));
            video_sdp.set_dir(media.get_reverse_dir());
            video_sdp.set_setup(SetupAttr(ROLE_PASSIVE));
            video_sdp.set_mid_attr(media.get_mid_attr().value());
            video_sdp.add_candidate(Candidate("fund_common", 1, "UDP", 2130706431, webrtc_session->get_local_ip(), webrtc_session->get_udp_port(), Candidate::CAND_TYPE_HOST, "", 0, {{"generation", "0"}}));
            video_sdp.set_rtcp_mux(RtcpMux());
            if (media.get_ssrc_group())
            {
                video_sdp.set_ssrc_group(media.get_ssrc_group().value());
            }

            for (auto &p : media.get_ssrcs())
            {
                video_sdp.add_ssrc(Ssrc(p.second.get_id(), webrtc_session->get_session_name(), webrtc_session->get_session_name(), webrtc_session->get_session_name() + "_video"));
                video_ssrc_ = p.second.get_id();
            }

            video_sdp.set_finger_print(FingerPrint("sha-1", webrtc_session->get_dtls_cert()->get_finger_print()));

            Payload *match_video_payload = nullptr;
            auto &payloads = media.get_payloads();
            for (auto &p : payloads)
            {
                if (p.second.get_encoding_name() != "H264")
                {
                    continue;
                }

                auto &fmtps = p.second.get_fmtps();
                for (auto &pair : fmtps)
                {
                    auto &fmtp = pair.second;
                    if (fmtp.get_param("packetization-mode") == "1" && fmtp.get_param("level-asymmetry-allowed") == "1"/*&& fmtp.get_param("profile-level-id") == "42001f"*/)
                    {
                        match_video_payload = (Payload *)&p.second;
                        break;
                    }
                }
            }

            if (!match_video_payload)
            {
                return -13;
            }

            video_pt_ = match_video_payload->get_pt();
            video_sdp.add_fmt(video_pt_);
            Payload video_payload(video_pt_, match_video_payload->get_encoding_name(), match_video_payload->get_clock_rate(), match_video_payload->get_encoding_params());
            video_payload.add_rtcp_fb(RtcpFb(video_pt_, "ccm", "fir"));
            video_payload.add_rtcp_fb(RtcpFb(video_pt_, "goog-remb"));
            video_payload.add_rtcp_fb(RtcpFb(video_pt_, "nack"));
            video_payload.add_rtcp_fb(RtcpFb(video_pt_, "nack", "pli"));
            video_payload.add_rtcp_fb(RtcpFb(video_pt_, "transport-cc"));
            for (auto &p : match_video_payload->get_fmtps())
            {
                video_payload.add_fmtp(p.second);
            }

            video_sdp.add_payload(video_payload);
            local_sdp_.add_media_sdp(video_sdp);
        }
    }
    return 0;
}

void WebRtcMediaSource::set_play_offer_sdp(std::shared_ptr<Sdp> sdp) {
    play_offer_sdp_ = sdp;
}

void WebRtcMediaSource::create_play_sdp() {
    Sdp play_sdp = local_sdp_;
    auto &play_medias = play_sdp.get_media_sdps();
    for (auto &media : play_medias) {
        media.set_dir(media.get_reverse_dir());
    }
    play_offer_sdp_ = std::make_shared<Sdp>();
    *play_offer_sdp_ = play_sdp;
}

bool WebRtcMediaSource::process_play_sdp(const std::string & sdp) {
    (void)sdp;
    return true;
}

Payload* WebRtcMediaSource::find_suitable_video_payload(MediaSdp & media_sdp) {
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
                    match_payload = (Payload *)(&p.second);
                    break;
                }
            }
        }

        if (match_payload) {
            break;
        }
    }
    return match_payload;
}

Payload* WebRtcMediaSource::find_suitable_audio_payload(MediaSdp & media_sdp) {
    Payload *match_payload = nullptr;
    auto & payloads  = media_sdp.get_payloads();
    for (auto & p : payloads) {
        std::string encoder = p.second.get_encoding_name();
        std::transform(encoder.begin(), encoder.end(), encoder.begin(), ::toupper); //将小写的都转换成大写
        if (encoder == "MPEG4-GENERIC") {//aac fmtp中streamtype必须是5才是audiostream，iso-14496-1 table 6中说明streamtype的作用
            match_payload = (Payload *)(&p.second);
        } else if (encoder == "OPUS") {
            match_payload = (Payload *)(&p.second);
        }

        if (match_payload) {
            break;
        }
    }
    return match_payload;
}

bool WebRtcMediaSource::create_video_codec_by_sdp(const MediaSdp & sdp, const Payload & payload) {
    std::string encoder = payload.get_encoding_name();
    std::transform(encoder.begin(), encoder.end(), encoder.begin(), ::toupper); //将小写的都转换成大写
    if (encoder == "H264") {
        video_codec_ = H264Codec::create_from_sdp(sdp, payload);
        if (!video_codec_) {
            return false;
        }
        video_codec_->set_data_rate(sdp.get_bw().get_value());

        // auto h264_codec = (H264Codec*)video_codec_.get();
        // uint32_t width, height;
        // bool ret = h264_codec->get_wh(width, height);
        // if (!ret) {
        //     return false;
        // }
        return true;
    } else {
        return false;
    }
    
    return false;
}

bool WebRtcMediaSource::create_audio_codec_by_sdp(const MediaSdp & sdp, const Payload & payload) {
    std::string encoder = payload.get_encoding_name();
    std::transform(encoder.begin(), encoder.end(), encoder.begin(), ::toupper); //将小写的都转换成大写
    if (encoder == "MPEG4-GENERIC") {
        audio_codec_ = AACCodec::create_from_sdp(sdp, payload);
        if (!audio_codec_) {
            return false;
        }
        audio_codec_->set_data_rate(sdp.get_bw().get_value());

        // auto aac_codec = (AACCodec*)video_codec_.get();
    } else if (encoder == "OPUS") {
        audio_codec_ = OpusCodec::create_from_sdp(sdp, payload);
        if (!audio_codec_) {
            return false;
        }
    }
    return true;
}

std::shared_ptr<MediaBridge> WebRtcMediaSource::get_or_create_bridge(const std::string & id, std::shared_ptr<PublishApp> app, const std::string & stream_name) {
    std::unique_lock<std::shared_mutex> lck(bridges_mtx_);
    std::shared_ptr<MediaBridge> bridge;
    auto it = bridges_.find(id);
    if (it != bridges_.end()) {
        bridge = it->second;
    } 

    if (bridge) {
        return bridge;
    }

    bridge = BridgeFactory::create_bridge(worker_, id, 
                                          app, std::weak_ptr<MediaSource>(shared_from_this()), 
                                          app->get_domain_name(), 
                                          app->get_app_name(), 
                                          stream_name);
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