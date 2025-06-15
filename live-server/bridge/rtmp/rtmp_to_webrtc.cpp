#include "spdlog/spdlog.h"
#include <memory>
#include <string_view>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>


#include "rtmp_to_webrtc.hpp"
#include "base/thread/thread_worker.hpp"
#include "core/flv_media_source.hpp"
#include "codec/h264/h264_codec.hpp"
#include "codec/aac/aac_codec.hpp"
#include "codec/aac/aac_decoder.hpp"
#include "codec/opus/opus_encoder.hpp"

#include "base/utils/utils.h"
#include "server/webrtc/webrtc_server.hpp"
#include "config/config.h"
#include "config/app_config.h"
#include "config/webrtc/webrtc_config.h"
#include "app/publish_app.h"

using namespace mms;

RtmpToWebRtc::RtmpToWebRtc(ThreadWorker *worker, std::shared_ptr<PublishApp> app, 
                           std::weak_ptr<MediaSource> origin_source, const std::string & domain_name, const std::string & app_name, const std::string & stream_name) : 
                           MediaBridge(worker, app, origin_source, domain_name, app_name, stream_name), check_closable_timer_(worker->get_io_context()), wg_(worker) {
    source_ = std::make_shared<WebRtcMediaSource>(worker, std::weak_ptr<StreamSession>(std::shared_ptr<StreamSession>(nullptr)), publish_app_);
    webrtc_media_source_ = std::static_pointer_cast<WebRtcMediaSource>(source_);
    sink_ = std::make_shared<RtmpMediaSink>(worker); 
    rtmp_media_sink_ = std::static_pointer_cast<RtmpMediaSink>(sink_);
}

RtmpToWebRtc::~RtmpToWebRtc() {
    spdlog::debug("destroy RtmpToWebRtc");
    if (swr_context_) {
        swr_free(&swr_context_);
        swr_context_ = nullptr;
    }
}

bool RtmpToWebRtc::init() {
    auto self(shared_from_this());
    wg_.add(1);
    boost::asio::co_spawn(worker_->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        boost::system::error_code ec;
         auto app_conf = publish_app_->get_conf();
        while (1) {
            check_closable_timer_.expires_from_now(std::chrono::milliseconds(app_conf->bridge_config().no_players_timeout_ms()));//10s检查一次
            co_await check_closable_timer_.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            if (boost::asio::error::operation_aborted == ec) {
                break;
            }

            if (webrtc_media_source_->has_no_sinks_for_time(app_conf->bridge_config().no_players_timeout_ms())) {//已经10秒没人播放了
                spdlog::debug("close RtmpToWebRtc because no players for 10s");
                close();
                break;
            }
        }
        co_return;
    }, [this, self](std::exception_ptr exp) {
        (void)exp;
        wg_.done();
        close();
    });

    rtmp_media_sink_->on_close([this, self]() {
        close();
    });

    rtmp_media_sink_->set_on_source_status_changed_cb(
        [this, self](SourceStatus status) -> boost::asio::awaitable<void> {
            webrtc_media_source_->set_status(status);
            if (status == E_SOURCE_STATUS_OK) {
                rtmp_media_sink_->on_rtmp_message(
                    [this, self](const std::vector<std::shared_ptr<RtmpMessage>> &rtmp_msgs)
                        -> boost::asio::awaitable<bool> {
                        for (auto rtmp_msg : rtmp_msgs) {
                            if (rtmp_msg->get_message_type() == RTMP_MESSAGE_TYPE_AUDIO) {
                                if (!co_await on_audio_packet(rtmp_msg)) {
                                    co_return false;
                                }
                            } else if (rtmp_msg->get_message_type() == RTMP_MESSAGE_TYPE_VIDEO) {
                                if (!co_await on_video_packet(rtmp_msg)) {
                                    co_return false;
                                }
                            } else {
                                if (!co_await on_metadata(rtmp_msg)) {
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

boost::asio::awaitable<bool> RtmpToWebRtc::on_audio_packet(std::shared_ptr<RtmpMessage> audio_pkt) {
    AudioTagHeader header;
    auto payload = audio_pkt->get_using_data();
    int32_t header_consumed = header.decode((uint8_t *)payload.data(), payload.size());
    if (header_consumed < 0) {
        co_return false;
    }

    AACCodec *aac_codec = ((AACCodec*)audio_codec_.get());
    bool sequence_header = false;
    if (header.is_seq_header()) {// 关键帧索引
        audio_ready_ = true;
        audio_header_ = audio_pkt;
        sequence_header = true;
        // 解析aac configuration header
        std::shared_ptr<AudioSpecificConfig> audio_config = std::make_shared<AudioSpecificConfig>();
        int32_t consumed = audio_config->parse((uint8_t*)payload.data() + header_consumed, payload.size() - header_consumed);
        if (consumed < 0) {
            spdlog::error("parse aac audio header failed, ret:{}", consumed);
            co_return false;
        }

        aac_codec->set_audio_specific_config(audio_config);

        aac_decoder_ = std::make_shared<AACDecoder>();
        if (!aac_decoder_->init(std::string(payload.data() + header_consumed, payload.size() - header_consumed))) {
            spdlog::error("init aac decoder failed");
            co_return false;
        }

        if (!swr_context_) {
            AVChannelLayout out_ch_layout;
            AVChannelLayout in_ch_layout;
            av_channel_layout_default(&out_ch_layout, 2);
            av_channel_layout_default(&in_ch_layout, audio_config->channel_configuration);
            int ret = swr_alloc_set_opts2(&swr_context_,
                                &out_ch_layout, AV_SAMPLE_FMT_S16, 48000,
                                &in_ch_layout, AV_SAMPLE_FMT_S16, audio_config->sampling_frequency, 
                                0, NULL);
            if (ret < 0) {
                spdlog::error("swr init failed");
                co_return false;
            }
        }

        if (swr_init(swr_context_) < 0) {
            spdlog::error("swr init failed");
            co_return false;
        }
        
        opus_encoder_ = std::make_shared<MOpusEncoder>();
        opus_encoder_->init(48000, 2);
        co_return true;
    }

    if (sequence_header) {
        co_return true;
    }

    if (!audio_ready_) {
        co_return false;
    }

    if (first_audio_pts_ == 0) {
        first_audio_pts_ = audio_pkt->timestamp_;
    }

    if (aac_decoder_) {
        auto pcm = aac_decoder_->decode((uint8_t*)payload.data() + header_consumed, payload.size() - header_consumed);
        if (swr_context_) {
            // int dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_context_, 48000) + in_samples, 44100, 48000, AV_ROUND_UP);
            uint8_t *data_in[1];
            uint8_t *data_out[1];
            data_in[0] = (uint8_t*)pcm.data();
            data_out[0] = (uint8_t*)resampled_pcm_ + resampled_pcm_bytes_;
            int32_t total_samples = 0;
            int32_t in_samples = pcm.size()/4;
            int out_samples = swr_convert(swr_context_, (uint8_t**)&data_out, 2048, (const uint8_t**)&data_in, in_samples);
            total_samples += out_samples;

            resampled_pcm_bytes_ += total_samples*2*2;
            std::vector<std::shared_ptr<RtpPacket>> rtp_pkts;
            if (opus_encoder_) {
                // frme_size：每个通道中输入音频信号的样本数，这里不是传pcm 数组大小，比如采用的是 48000 hz 编码，20ms 帧长，那么frame_size 应该是48*20 = 960，pcm 分配大小= frame_size x channels x sizeof(ipus_int16)。
                const int samples_20ms = 48*20;//960//
                const int bytes_per_20ms = samples_20ms *2 * 2;
                while (resampled_pcm_bytes_ >= bytes_per_20ms) {
                    auto opus_bytes = opus_encoder_->encode((uint8_t*)resampled_pcm_, samples_20ms, opus_data_, 8192);
                    resampled_pcm_bytes_ -= bytes_per_20ms;
                    memmove(resampled_pcm_, resampled_pcm_ + bytes_per_20ms, resampled_pcm_bytes_);
                    auto rtp_pkts_tmp = audio_rtp_packer_.pack((char*)opus_data_, opus_bytes, audio_pt_, audio_ssrc_, audio_sample_count_*20*48);
                    audio_sample_count_++;
                    if (rtp_pkts_tmp.size() > 0) {
                        for (auto r : rtp_pkts_tmp) {
                            rtp_pkts.push_back(r);
                        }
                    }
                }
                auto ret = co_await webrtc_media_source_->on_audio_packets(rtp_pkts);
                if (!ret) {
                    co_return false;
                }
            }
        }
    }
    
    co_return true;
}

int32_t RtmpToWebRtc::get_nalus(uint8_t *data, int32_t len, std::list<std::string_view> & nalus) {
    uint8_t *data_start = data;
    while (len > 0) {
        int32_t nalu_len = 0;
        if (len < nalu_length_size_) {
            return -1;
        }

        if (nalu_length_size_ == 1) {
            nalu_len = *data;
        } else if (nalu_length_size_ == 2) {
            nalu_len = ntohs((*(uint16_t *)data));
        } else if (nalu_length_size_ == 4) {
            nalu_len = ntohl(*(uint32_t *)data);
        }

        data += nalu_length_size_;
        len -= nalu_length_size_;

        if (len < nalu_len) {
            return -2;
        }

        nalus.emplace_back(std::string_view((char*)data, nalu_len));
        data += nalu_len;
        len -= nalu_len;
    }
    return data - data_start;
}

boost::asio::awaitable<bool> RtmpToWebRtc::on_video_packet(std::shared_ptr<RtmpMessage> video_pkt) {
    auto payload = video_pkt->get_using_data();
    VideoTagHeader header;
    int32_t header_consumed = header.decode((uint8_t *)payload.data(), payload.size());
    if (header_consumed < 0) {
        co_return false;
    }

    H264Codec *h264_codec = ((H264Codec*)video_codec_.get());
    if (header.is_seq_header()) {// 关键帧索引
        video_header_ = video_pkt;
        // 解析avc configuration heade
        AVCDecoderConfigurationRecord avc_decoder_configuration_record;
        int32_t consumed = avc_decoder_configuration_record.parse((uint8_t*)payload.data() + header_consumed, payload.size() - header_consumed);
        if (consumed < 0) {
            co_return false;
        }

        h264_codec->set_sps_pps(avc_decoder_configuration_record.get_sps(), avc_decoder_configuration_record.get_pps());
        nalu_length_size_ = avc_decoder_configuration_record.nalu_length_size_minus_one + 1;
        if (nalu_length_size_ == 3 || nalu_length_size_ > 4) {
            co_return false;
        }
        co_return true;
    } 

    // bool is_key = header.is_key_frame() && !header.is_seq_header();
    // 获取到nalus
    std::list<std::string_view> nalus;
    auto consumed = get_nalus((uint8_t*)payload.data() + header_consumed, payload.size() - header_consumed, nalus);
    if (consumed < 0) {
        co_return false;
    }

    // 判断sps,pps,aud等
    bool has_aud_nalu = false;
    bool has_sps_nalu = false;
    bool has_pps_nalu = false;
    if (!video_codec_) {
        co_return false;
    }

    if (first_video_pts_ == 0) {
        first_video_pts_ = video_pkt->timestamp_;
    }

    for (auto it = nalus.begin(); it != nalus.end(); it++) {
        H264NaluType nalu_type = H264NaluType((*it->data()) & 0x1f);
        if (nalu_type == H264NaluTypeAccessUnitDelimiter) {
            has_aud_nalu = true;
        } else if (nalu_type == H264NaluTypeSPS) {
            has_sps_nalu = true;
            h264_codec->set_sps(std::string((it)->data(), (it)->size()));
        } else if (nalu_type == H264NaluTypePPS) {
            has_pps_nalu = true;
            h264_codec->set_pps(std::string((it)->data(), (it)->size()));
        } else if (nalu_type == H264NaluTypeIDR) {
            if (!has_pps_nalu && !has_sps_nalu) {
                it = nalus.insert(it, std::string_view(h264_codec->get_pps_nalu().data(), h264_codec->get_pps_nalu().size()));
                it = nalus.insert(it, std::string_view(h264_codec->get_sps_nalu().data(), h264_codec->get_sps_nalu().size()));
                has_pps_nalu = true;
                has_sps_nalu = true;
            }
        }
    }

    static uint8_t default_aud_nalu[] = { 0x09, 0xf0 };
    static std::string_view aud_nalu((char*)default_aud_nalu, 2);
    if (!has_aud_nalu) {
        nalus.push_front(aud_nalu);
    }

    auto rtp_pkts = video_rtp_packer_.pack(nalus, video_pt_, video_ssrc_, (video_pkt->timestamp_ - first_video_pts_)*90);
    bool ret = co_await webrtc_media_source_->on_video_packets(rtp_pkts);
    if (!ret) {
        co_return false;
    }
    
    // for (auto rtp_pkt : rtp_pkts) {
    //     spdlog::info("send pt:{:x}, num:{} by bridge", rtp_pkt->get_pt(), rtp_pkt->get_seq_num());
    //     bool ret = co_await webrtc_media_source_->on_video_packets(rtp_pkts);
    //     if (!ret) {
    //         co_return false;
    //     }
    // }

    co_return true;
}

boost::asio::awaitable<bool> RtmpToWebRtc::on_metadata(std::shared_ptr<RtmpMessage> metadata_pkt) {
    metadata_ = std::make_shared<RtmpMetaDataMessage>();
    if (metadata_->decode(metadata_pkt) <= 0) {
        co_return false;
    }
    has_video_ = metadata_->has_video();
    has_audio_ = metadata_->has_audio();

    if (has_video_) {
        auto video_codec_id = metadata_->get_video_codec_id();
        if (video_codec_id == VideoTagHeader::AVC) {
            video_codec_ = std::make_shared<H264Codec>();
        } else {
            co_return false;
        }
    }

    if (has_audio_) {
        auto audio_codec_id = metadata_->get_audio_codec_id();
        if (audio_codec_id == AudioTagHeader::AAC) {
            audio_codec_ = std::make_shared<AACCodec>();;
        } else {
            co_return false;
        }
    }

    std::string session_name = domain_name_ + "/" + app_name_ + "/" + stream_name_;
    play_offer_sdp_ = std::make_shared<Sdp>();
    play_offer_sdp_->set_version(0);
    play_offer_sdp_->set_origin({"-", Utils::get_rand64(), 1, "IN", "IP4", "127.0.0.1"}); // o=- get_rand64 1 IN IP4 127.0.0.1
    play_offer_sdp_->set_session_name(session_name);                                  //
    play_offer_sdp_->set_time({0, 0});                                                // t=0 0
    play_offer_sdp_->set_tool({"mms"});
    if (has_audio_ && has_video_) {
        play_offer_sdp_->set_bundle({"0", "1"});
    }
    
    play_offer_sdp_->add_attr("ice-lite");
    play_offer_sdp_->add_attr("msid-semantic: WMS " + session_name);
    auto config = Config::get_instance();
    if (has_audio_) {
        MediaSdp audio_sdp;
        audio_sdp.set_media("audio");
        audio_sdp.set_port(9);
        audio_sdp.set_port_count(1);
        audio_sdp.set_proto("UDP/TLS/RTP/SAVPF");
        audio_sdp.add_fmt(audio_pt_);
        audio_sdp.set_connection_info({"IN", "IP4", "0.0.0.0"});
        // audio_sdp.set_ice_ufrag(IceUfrag(webrtc_session->get_local_ice_ufrag()));//这两个值在session处理时，会被设置上
        // audio_sdp.set_ice_pwd(IcePwd(webrtc_session->get_local_ice_pwd()));
        audio_sdp.set_dir(DirAttr(DirAttr::MEDIA_SENDONLY));
        audio_sdp.set_setup(SetupAttr(ROLE_PASSIVE));
        audio_sdp.set_mid_attr(MidAttr("0"));
        audio_sdp.set_rtcp_mux(RtcpMux());
        auto &webrtc_config = config->get_webrtc_config();
        audio_sdp.add_candidate(Candidate("fund_common", 1, "UDP", 2130706431, 
                                webrtc_config.get_ip(), 
                                webrtc_config.get_udp_port(), 
                                Candidate::CAND_TYPE_HOST, "", 0, {{"generation", "0"}}));
        // if (media.get_ssrc_group())
        // {
        //     audio_sdp.set_ssrc_group(media.get_ssrc_group().value());
        // }

        // for (auto &p : media.get_ssrcs())
        // {
        //     audio_sdp.add_ssrc(Ssrc(p.second.get_id(), webrtc_session->get_session_name(), webrtc_session->get_session_name(), webrtc_session->get_session_name() + "_audio"));
        //     audio_ssrc_ = p.second.get_id();
        // }
        audio_sdp.add_ssrc(Ssrc(audio_ssrc_, session_name, session_name, session_name + "_audio"));

        audio_sdp.set_finger_print(FingerPrint("sha-1", WebRtcServer::get_default_dtls_cert()->get_finger_print()));
        Payload audio_payload(audio_pt_, "OPUS", 48000, {"2"});
        audio_payload.add_rtcp_fb(RtcpFb(audio_pt_, "nack"));
        audio_payload.add_rtcp_fb(RtcpFb(audio_pt_, "nack", "pli"));
        audio_sdp.add_payload(audio_payload);
        play_offer_sdp_->add_media_sdp(audio_sdp);
    }

    if (has_video_) {
        MediaSdp video_sdp;
        video_sdp.set_media("video");
        video_sdp.set_port(9);
        video_sdp.set_port_count(1);
        video_sdp.set_proto("UDP/TLS/RTP/SAVPF");
        video_sdp.set_connection_info({"IN", "IP4", "0.0.0.0"});
        // video_sdp.set_ice_ufrag(IceUfrag(webrtc_session->get_local_ice_ufrag()));
        // video_sdp.set_ice_pwd(IcePwd(webrtc_session->get_local_ice_pwd()));
        video_sdp.set_dir(DirAttr(DirAttr::MEDIA_SENDONLY));
        video_sdp.set_setup(SetupAttr(ROLE_PASSIVE));
        video_sdp.set_mid_attr(MidAttr("1"));
        auto &webrtc_config = config->get_webrtc_config();
        video_sdp.add_candidate(Candidate("fund_common", 1, "UDP", 2130706431, 
                                webrtc_config.get_ip(), 
                                webrtc_config.get_udp_port(), 
                                Candidate::CAND_TYPE_HOST, "", 0, {{"generation", "0"}}));
        video_sdp.set_rtcp_mux(RtcpMux());
        // if (media.get_ssrc_group())
        // {
        //     video_sdp.set_ssrc_group(media.get_ssrc_group().value());
        // }

        // for (auto &p : media.get_ssrcs())
        // {
        //     video_sdp.add_ssrc(Ssrc(p.second.get_id(), webrtc_session->get_session_name(), webrtc_session->get_session_name(), webrtc_session->get_session_name() + "_video"));
        //     video_ssrc_ = p.second.get_id();
        // }
        // SsrcGroup ssrc_group;
        // ssrc_group.semantics = "FID";
        auto video_ssrc = Ssrc(video_ssrc_, session_name, session_name, session_name + "_video");
        // auto audio_ssrc = Ssrc(audio_ssrc_, session_name, session_name, session_name + "_audio");
        // ssrc_group.add_ssrc(video_ssrc);
        // ssrc_group.add_ssrc(audio_ssrc);
        // video_sdp.set_ssrc_group(ssrc_group);
        video_sdp.add_ssrc(video_ssrc);
        video_sdp.set_finger_print(FingerPrint("sha-1", WebRtcServer::get_default_dtls_cert()->get_finger_print()));
        video_sdp.add_fmt(video_pt_);
        Payload video_payload(video_pt_, "H264", 90000, {});
        video_payload.add_rtcp_fb(RtcpFb(video_pt_, "ccm", "fir"));
        video_payload.add_rtcp_fb(RtcpFb(video_pt_, "goog-remb"));
        video_payload.add_rtcp_fb(RtcpFb(video_pt_, "nack"));
        video_payload.add_rtcp_fb(RtcpFb(video_pt_, "nack", "pli"));
        video_payload.add_rtcp_fb(RtcpFb(video_pt_, "transport-cc"));
        Fmtp fmtp;
        fmtp.set_pt(video_pt_);
        fmtp.add_param("packetization-mode", "1");
        fmtp.add_param("level-asymmetry-allowed", "1");
        fmtp.add_param("profile-level-id", "42001f");
        video_payload.add_fmtp(fmtp);

        video_sdp.add_payload(video_payload);
        play_offer_sdp_->add_media_sdp(video_sdp);
    }

    webrtc_media_source_->set_play_offer_sdp(play_offer_sdp_);
    co_return true;
} 

void RtmpToWebRtc::close() {
    if (closed_.test_and_set(std::memory_order_acquire)) {
        return;
    }

    auto self(shared_from_this());
    boost::asio::co_spawn(worker_->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        check_closable_timer_.cancel();
        co_await wg_.wait();
        if (webrtc_media_source_) {
            webrtc_media_source_->close();
            webrtc_media_source_ = nullptr;
        }

        auto origin_source = origin_source_.lock();
        if (rtmp_media_sink_) {
            rtmp_media_sink_->on_close({});
            rtmp_media_sink_->on_rtmp_message({});
            rtmp_media_sink_->set_on_source_status_changed_cb({});
            rtmp_media_sink_->close();
            if (origin_source) {
                origin_source->remove_media_sink(rtmp_media_sink_);
            }
            rtmp_media_sink_ = nullptr;
        }

        if (origin_source) {
            origin_source->remove_bridge(shared_from_this());
        }
    }, boost::asio::detached);
}