#include "webrtc_to_rtmp.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>

#include "base/thread/thread_worker.hpp"
#include "codec/aac/aac_codec.hpp"
#include "codec/aac/aac_encoder.hpp"
#include "codec/codec.hpp"
#include "codec/h264/h264_codec.hpp"
#include "codec/opus/opus_codec.hpp"
#include "core/rtmp_media_source.hpp"
#include "core/rtp_media_sink.hpp"


using namespace mms;

WebRtcToRtmp::WebRtcToRtmp(ThreadWorker *worker, std::shared_ptr<PublishApp> app,
                           std::weak_ptr<MediaSource> origin_source, const std::string &domain_name,
                           const std::string &app_name, const std::string &stream_name)
    : MediaBridge(worker, app, origin_source, domain_name, app_name, stream_name),
      check_closable_timer_(worker->get_io_context()),
      wg_(worker) {
    source_ = std::make_shared<RtmpMediaSource>(
        worker, std::weak_ptr<StreamSession>(std::shared_ptr<StreamSession>(nullptr)), publish_app_);
    rtmp_media_source_ = std::static_pointer_cast<RtmpMediaSource>(source_);
    sink_ = std::make_shared<RtpMediaSink>(worker);
    rtp_media_sink_ = std::static_pointer_cast<RtpMediaSink>(sink_);
    video_frame_cache_ = std::make_unique<char[]>(1024 * 1024);
    audio_frame_cache_ = std::make_unique<char[]>(1024 * 20);
    type_ = "webrtc-to-rtmp";
}

WebRtcToRtmp::~WebRtcToRtmp() {
    spdlog::debug("destroy WebRtcToRtmp");
    if (swr_context_) {
        swr_free(&swr_context_);
        swr_context_ = nullptr;
    }
}

bool WebRtcToRtmp::init() {
    auto self(shared_from_this());
    wg_.add(1);
    boost::asio::co_spawn(
        worker_->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            boost::system::error_code ec;
            while (1) {
                check_closable_timer_.expires_after(std::chrono::milliseconds(30000));  // 30s检查一次
                co_await check_closable_timer_.async_wait(
                    boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                if (boost::asio::error::operation_aborted == ec) {
                    break;
                }

                if (rtmp_media_source_->has_no_sinks_for_time(30000)) {  // 已经30秒没人播放了
                    spdlog::debug("close WebRtcToRtmp because no players for 30s");
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

    rtp_media_sink_->set_source_codec_ready_cb([this, self](std::shared_ptr<Codec> video_codec,
                                                            std::shared_ptr<Codec> audio_codec) -> bool {
        video_codec_ = video_codec;
        audio_codec_ = audio_codec;
        if (video_codec_) {
            has_video_ = true;
        }

        if (audio_codec_) {
            if (audio_codec_->get_codec_type() != CODEC_OPUS) {
                return false;
            }

            OpusCodec *opus_codec = (OpusCodec *)audio_codec_.get();
            opus_decoder_ = opus_codec->create_decoder(opus_codec->getFs(), opus_codec->get_channels());
            if (!swr_context_) {
                AVChannelLayout out_ch_layout;
                AVChannelLayout in_ch_layout;
                av_channel_layout_default(&out_ch_layout, 2);
                av_channel_layout_default(&in_ch_layout, opus_codec->get_channels());
                int ret = swr_alloc_set_opts2(&swr_context_, &out_ch_layout, AV_SAMPLE_FMT_S16, 44100,
                                              &in_ch_layout, AV_SAMPLE_FMT_S16, opus_codec->getFs(), 0, NULL);
                if (ret < 0) {
                    spdlog::error("swr init failed");
                    return false;
                }
            }
            if (swr_init(swr_context_) < 0) {
                spdlog::error("swr init failed");
                return false;
            }

            aac_encoder_ = std::make_unique<AACEncoder>();
            aac_encoder_->init(44100, opus_codec->get_channels());
            has_audio_ = true;
        }

        if (video_codec_ && !video_codec_->is_ready()) {
            return true;
        }

        if (audio_codec_ && !audio_codec_->is_ready()) {
            return true;
        }

        return generate_rtmp_headers();
    });

    rtp_media_sink_->set_video_pkts_cb(
        [this, self](std::vector<std::shared_ptr<RtpPacket>> pkts) -> boost::asio::awaitable<bool> {
            if (first_rtp_video_ts_ == 0) {
                if (pkts.size() > 0) {
                    first_rtp_video_ts_ = pkts[0]->get_timestamp();
                }
            }

            for (auto pkt : pkts) {
                process_video_packet(pkt);
            }

            co_return true;
        });

    rtp_media_sink_->set_audio_pkts_cb(
        [this, self](std::vector<std::shared_ptr<RtpPacket>> pkts) -> boost::asio::awaitable<bool> {
            if (first_rtp_audio_ts_ == 0) {
                if (pkts.size() > 0) {
                    first_rtp_audio_ts_ = pkts[0]->get_timestamp();
                }
            }

            for (auto pkt : pkts) {
                process_audio_packet(pkt);
            }

            co_return true;
        });

    return true;
}

bool WebRtcToRtmp::generate_rtmp_headers() {
    if (!generate_metadata()) {
        return false;
    }

    if (!rtmp_media_source_->on_metadata(metadata_pkt_)) {
        return false;
    }

    if (!generate_video_header()) {
        return false;
    }

    if (!rtmp_media_source_->on_video_packet(video_header_)) {
        return false;
    }

    if (!generate_audio_header()) {
        return false;
    }

    if (!rtmp_media_source_->on_audio_packet(audio_header_)) {
        return false;
    }

    header_ready_ = true;
    return true;
}

bool WebRtcToRtmp::generate_metadata() {
    /*
    {
        "2.1" : false,
        "3.1" : false,
        "4.0" : false,
        "4.1" : false,
        "5.1" : false,
        "7.1" : false,
        "audiochannels" : 2.0,
        "audiocodecid" : 10.0,
        "audiodatarate" : 160.0,
        "audiosamplerate" : 44100.0,
        "audiosamplesize" : 16.0,
        "duration" : 0.0,
        "encoder" : "obs-output module (libobs version 28.1.0)",
        "fileSize" : 0.0,
        "framerate" : 30.0,
        "height" : 720.0,
        "stereo" : true,
        "videocodecid" : 7.0,
        "videodatarate" : 2500.0,
        "width" : 1280.0
    }
    */
    Amf0String name1;
    name1.set_value("@setDataFrame");
    Amf0String name2;
    name2.set_value("onMetaData");

    metadata_amf0_.set_item_value("2.1", false);
    metadata_amf0_.set_item_value("3.1", false);
    metadata_amf0_.set_item_value("4.0", false);
    metadata_amf0_.set_item_value("4.1", false);
    metadata_amf0_.set_item_value("5.1", false);
    metadata_amf0_.set_item_value("7.1", false);
    metadata_amf0_.set_item_value("audiocodecid", 10.0);  // aac
    // 固定值
    AudioSpecificConfig audio_specific_config;
    audio_specific_config.audio_object_type = AOT_AAC_LC;
    audio_specific_config.channel_configuration = 2;
    audio_specific_config.frequency_index = 4;  // 44100
    audio_specific_config.sampling_frequency = 44100;

    metadata_amf0_.set_item_value("audiochannels", (double)2);
    if (audio_specific_config.channel_configuration >= 2) {
        metadata_amf0_.set_item_value("stereo", true);
    } else {
        metadata_amf0_.set_item_value("stereo", false);
    }
    metadata_amf0_.set_item_value("audiodatarate", 2500);
    metadata_amf0_.set_item_value("audiosamplesize", 16.0);  // 好像aac是固定的16bit
    metadata_amf0_.set_item_value("duration", 0.0);
    metadata_amf0_.set_item_value("encoder", "mms");  // todo:add version
    metadata_amf0_.set_item_value("fileSize", 0.0);

    if (video_codec_->get_codec_type() == CODEC_H264) {
        auto h264_codec = std::static_pointer_cast<H264Codec>(video_codec_);
        metadata_amf0_.set_item_value("videocodecid", 7.0);
        uint32_t w, h;
        h264_codec->get_wh(w, h);
        metadata_amf0_.set_item_value("width", double(w));
        metadata_amf0_.set_item_value("height", double(h));
        double fps;
        if (h264_codec->get_fps(fps)) {
            metadata_amf0_.set_item_value("framerate", fps);
        }
        metadata_amf0_.set_item_value("videodatarate", (double)video_codec_->get_data_rate());
    }

    auto total_size = name1.size() + name2.size() + metadata_amf0_.size();
    metadata_pkt_ = std::make_shared<RtmpMessage>(total_size);
    auto unuse_data = metadata_pkt_->get_unuse_data();
    int32_t consumed1 = name1.encode((uint8_t *)unuse_data.data(), unuse_data.size());
    metadata_pkt_->inc_used_bytes(consumed1);
    unuse_data = metadata_pkt_->get_unuse_data();
    int32_t consumed2 = name2.encode((uint8_t *)unuse_data.data(), unuse_data.size());
    metadata_pkt_->inc_used_bytes(consumed2);
    unuse_data = metadata_pkt_->get_unuse_data();
    int32_t consumed3 = metadata_amf0_.encode((uint8_t *)unuse_data.data(), unuse_data.size());
    metadata_pkt_->inc_used_bytes(consumed3);

    metadata_pkt_->message_stream_id_ = 0;
    metadata_pkt_->chunk_stream_id_ = 18;
    metadata_pkt_->message_type_id_ = FlvTagHeader::ScriptTag;
    metadata_pkt_->timestamp_ = 0;
    return true;
}

bool WebRtcToRtmp::generate_video_header() {
    if (video_codec_->get_codec_type() == CODEC_H264) {
        auto h264_codec = std::static_pointer_cast<H264Codec>(video_codec_);
        AVCDecoderConfigurationRecord &decode_configuration_record = h264_codec->get_avc_configuration();
        auto payload_size = decode_configuration_record.size();
        video_header_ = std::make_shared<RtmpMessage>(5 + payload_size);  // 5字节是video tag header的大小
        // rtmp头
        video_header_->message_stream_id_ = 0;
        video_header_->chunk_stream_id_ = 8;
        video_header_->message_type_id_ = FlvTagHeader::VideoTag;
        video_header_->timestamp_ = 0;
        // flv video tag header
        VIDEODATA video_data;
        video_data.header.frame_type = VideoTagHeader::KeyFrame;
        video_data.header.codec_id = VideoTagHeader::AVC;
        video_data.header.avc_packet_type = VideoTagHeader::AVCSequenceHeader;
        video_data.header.composition_time = 0;

        auto payload = video_header_->get_unuse_data();
        int32_t consumed1 = video_data.encode((uint8_t *)payload.data(), payload.size());
        if (consumed1 < 0) {
            spdlog::error("video header encode failed, consumed:{}", consumed1);
            return false;
        }

        video_header_->inc_used_bytes(consumed1);
        payload = video_header_->get_unuse_data();

        auto consumed2 = decode_configuration_record.encode((uint8_t *)payload.data(), payload.size());
        if (consumed2 < 0) {
            // spdlog::error("video configuration record encode failed");
        } else {
            spdlog::info("video configuration record encode succeed, consumed:{}, payload_size:{}", consumed2,
                         payload_size);
        }
        video_header_->inc_used_bytes(consumed2);
        video_data.payload = payload;
        spdlog::info("video_data.payload.size()={}", video_data.payload.size());
        return true;
    }

    return false;
}

bool WebRtcToRtmp::generate_audio_header() {
    if (audio_codec_->get_codec_type() == CODEC_AAC) {
        auto aac_codec = std::static_pointer_cast<AACCodec>(audio_codec_);
        AUDIODATA audio_data;
        audio_data.header.sound_format = AudioTagHeader::AAC;
        auto audio_config = aac_codec->get_audio_specific_config();
        if (!audio_config) {
            spdlog::error("could not find audio config");
            return false;
        }

        if (audio_config->sampling_frequency == 44100) {
            audio_data.header.sound_rate = AudioTagHeader::KHZ_44;
        } else if (audio_config->sampling_frequency == 11025) {
            audio_data.header.sound_rate = AudioTagHeader::KHZ_11;
        } else if (audio_config->sampling_frequency == 22050) {
            audio_data.header.sound_rate = AudioTagHeader::KHZ_22;
        } else {
            spdlog::error("could not find audio sampling_frequency:{}", audio_config->sampling_frequency);
            // return false;
            audio_data.header.sound_rate = AudioTagHeader::KHZ_22;
        }

        audio_data.header.sound_size = AudioTagHeader::Sample_16bit;
        audio_data.header.aac_packet_type = AudioTagHeader::AACSequenceHeader;

        auto payload_size = audio_config->size();
        audio_header_ = std::make_shared<RtmpMessage>(2 + payload_size);
        // rtmp header
        audio_header_->message_stream_id_ = 0;
        audio_header_->chunk_stream_id_ = 9;
        audio_header_->message_type_id_ = FlvTagHeader::AudioTag;
        audio_header_->timestamp_ = 0;
        // 音频头部
        auto payload = audio_header_->get_unuse_data();
        auto consumed1 = audio_data.encode((uint8_t *)payload.data(), payload.size());
        if (consumed1 < 0) {
            spdlog::error("audio data encode failed, consumed1:{}", consumed1);
            return false;
        }
        audio_header_->inc_used_bytes(consumed1);
        payload = audio_header_->get_unuse_data();

        auto consumed2 = audio_config->encode((uint8_t *)payload.data(), payload.size());
        if (consumed2 < 0) {
            spdlog::error("audio data encode failed, consumed2:{}", consumed2);
            return false;
        }

        audio_data.payload = payload;
        audio_header_->inc_used_bytes(consumed2);
        return true;
    }

    return false;
}

void WebRtcToRtmp::process_video_packet(std::shared_ptr<RtpPacket> pkt) {
    if (video_codec_ && video_codec_->get_codec_type() == CODEC_H264) {
        process_h264_packet(pkt);
    }
}

void WebRtcToRtmp::process_h264_packet(std::shared_ptr<RtpPacket> pkt) {
    auto h264_nalu = rtp_h264_depacketizer_.on_packet(pkt);
    if (h264_nalu) {
        uint32_t this_timestamp = h264_nalu->get_timestamp();
        auto rtmp_msg = generate_h264_rtmp_msg((this_timestamp - first_rtp_video_ts_) / 90,
                                               h264_nalu);  // todo 除90这个要根据sdp来计算，目前固定
        if (rtmp_msg) {
            rtmp_media_source_->on_video_packet(rtmp_msg);
        }
    }
}

std::shared_ptr<RtmpMessage> WebRtcToRtmp::generate_h264_rtmp_msg(uint32_t timestamp,
                                                                  std::shared_ptr<RtpH264NALU> &nalu) {
    bool is_key = false;
    char *buf = video_frame_cache_.get();
    auto &pkts = nalu->get_rtp_pkts();
    int32_t total_payload_size = 0;
    for (auto it = pkts.begin(); it != pkts.end(); it++) {
        auto pkt = it->second;
        H264RtpPktInfo pkt_info;
        pkt_info.parse(pkt->get_payload().data(), pkt->get_payload().size());

        if (pkt_info.is_stap_a()) {
            std::string_view payload = pkt->get_payload();
            const char *data = payload.data() + 1;
            size_t pos = 1;

            while (pos < payload.size()) {
                uint16_t nalu_size = ntohs(*(uint16_t *)data);
                uint8_t nalu_type = *(data + 2) & 0x1F;
                if (nalu_type == H264NaluTypeIDR) {
                    is_key = true;
                }

                uint32_t s = htonl(nalu_size);
                memcpy(buf, &s, 4);
                buf += 4;
                memcpy(buf, data + 2, nalu_size);
                buf += nalu_size;

                pos += 2 + nalu_size;
                data += 2 + nalu_size;
            }

            if (pkt->get_header().marker == 1) {  // 最后一个
                total_payload_size = buf - video_frame_cache_.get();
                std::shared_ptr<RtmpMessage> video_msg =
                    std::make_shared<RtmpMessage>(total_payload_size + 5);
                video_msg->message_stream_id_ = 0;
                video_msg->chunk_stream_id_ = 8;
                video_msg->message_type_id_ = FlvTagHeader::VideoTag;
                video_msg->timestamp_ = timestamp;

                VIDEODATA video_data;

                video_data.header.frame_type = is_key ? VideoTagHeader::KeyFrame : VideoTagHeader::InterFrame;
                video_data.header.codec_id = VideoTagHeader::AVC;
                video_data.header.avc_packet_type = VideoTagHeader::AVCNALU;
                video_data.header.composition_time = 0;  // rtp默认不支持b帧先

                auto payload = video_msg->get_unuse_data();
                video_data.payload = std::string_view((char *)payload.data() + 5, total_payload_size);
                memcpy((char *)payload.data() + 5, video_frame_cache_.get(), total_payload_size);
                auto ret = video_data.encode((uint8_t *)payload.data(), total_payload_size + 5);
                if (ret < 0) {
                    // spdlog::error("encode flv tag failed, code:{}", ret);
                }
                video_msg->inc_used_bytes(total_payload_size + 5);
                return video_msg;
            }
        } else if (pkt_info.is_single_nalu()) {
            if (pkt_info.get_nalu_type() == H264NaluTypeIDR) {
                is_key = true;
            }

            uint32_t s = htonl(pkt->get_payload().size());
            memcpy(buf, &s, 4);
            buf += 4;
            memcpy(buf, pkt->get_payload().data(), pkt->get_payload().size());
            buf += pkt->get_payload().size();

            if (pkt->get_header().marker == 1) {
                total_payload_size = buf - video_frame_cache_.get();
                std::shared_ptr<RtmpMessage> video_msg =
                    std::make_shared<RtmpMessage>(total_payload_size + 5);
                auto payload = video_msg->get_unuse_data();
                video_msg->message_stream_id_ = 0;
                video_msg->chunk_stream_id_ = 8;
                video_msg->message_type_id_ = FlvTagHeader::VideoTag;
                video_msg->timestamp_ = timestamp;

                VIDEODATA video_data;

                video_data.header.frame_type = is_key ? VideoTagHeader::KeyFrame : VideoTagHeader::InterFrame;
                video_data.header.codec_id = VideoTagHeader::AVC;
                video_data.header.avc_packet_type = VideoTagHeader::AVCNALU;
                video_data.header.composition_time = 0;  // rtp默认不支持b帧先

                video_data.payload = std::string_view((char *)payload.data() + 5, total_payload_size);
                memcpy((char *)payload.data() + 5, video_frame_cache_.get(), total_payload_size);
                auto ret = video_data.encode((uint8_t *)payload.data(), total_payload_size + 5);
                if (ret < 0) {
                    // spdlog::error("encode flv tag failed, code:{}", ret);
                }
                video_msg->inc_used_bytes(total_payload_size + 5);
                return video_msg;
            }
        } else if (pkt_info.get_type() == H264_RTP_PAYLOAD_FU_A) {
            int32_t nalu_size = 0;
            int32_t *nalu_size_buf_pos = (int32_t *)buf;
            buf += 4;
            if (pkt_info.is_start_fu()) {
                do {
                    if (pkt_info.is_start_fu()) {
                        if (pkt_info.get_nalu_type() == H264NaluTypeIDR) {
                            is_key = true;
                        }

                        nalu_size += pkt->get_payload().size() - 1;
                        const uint8_t *pkt_buf = (const uint8_t *)pkt->get_payload().data();
                        uint8_t nalu_type = (pkt_buf[0] & 0xe0) | (pkt_buf[1] & 0x1F);
                        memcpy(buf, &nalu_type, 1);
                        memcpy(buf + 1, pkt->get_payload().data() + 2, pkt->get_payload().size() - 2);
                        buf += pkt->get_payload().size() - 1;
                    } else {
                        nalu_size += pkt->get_payload().size() - 2;
                        memcpy(buf, pkt->get_payload().data() + 2, pkt->get_payload().size() - 2);
                        buf += pkt->get_payload().size() - 2;
                    }

                    if (pkt_info.is_end_fu()) {
                        *nalu_size_buf_pos = htonl(nalu_size);
                        break;
                    }
                    it++;

                    if (it != pkts.end()) {
                        pkt = it->second;
                        pkt_info.parse(pkt->get_payload().data(), pkt->get_payload().size());
                    }
                } while (pkt_info.get_type() == H264_RTP_PAYLOAD_FU_A && it != pkts.end());
            }

            if (pkt->get_header().marker == 1) {
                total_payload_size = buf - video_frame_cache_.get();
                std::shared_ptr<RtmpMessage> video_msg =
                    std::make_shared<RtmpMessage>(total_payload_size + 5);
                auto payload = video_msg->get_unuse_data();
                video_msg->message_stream_id_ = 0;
                video_msg->chunk_stream_id_ = 8;
                video_msg->message_type_id_ = FlvTagHeader::VideoTag;
                video_msg->timestamp_ = timestamp;

                VIDEODATA video_data;

                video_data.header.frame_type = is_key ? VideoTagHeader::KeyFrame : VideoTagHeader::InterFrame;
                video_data.header.codec_id = VideoTagHeader::AVC;
                video_data.header.avc_packet_type = VideoTagHeader::AVCNALU;
                video_data.header.composition_time = 0;  // rtp默认不支持b帧先

                video_data.payload = std::string_view((char *)payload.data() + 5, total_payload_size);
                memcpy((char *)payload.data() + 5, video_frame_cache_.get(), total_payload_size);
                auto ret = video_data.encode((uint8_t *)payload.data(), total_payload_size + 5);
                if (ret < 0) {
                    // spdlog::error("encode rtmp video message failed, code:{}", ret);
                }
                video_msg->inc_used_bytes(total_payload_size + 5);
                return video_msg;
            }
        }
    }
    return nullptr;
}

std::shared_ptr<RtmpMessage> WebRtcToRtmp::generate_aac_rtmp_msg(uint32_t timestamp) {
    std::shared_ptr<RtmpMessage> audio_msg = std::make_shared<RtmpMessage>(aac_bytes_ + 2);
    audio_msg->message_stream_id_ = 0;
    audio_msg->chunk_stream_id_ = 9;
    audio_msg->message_type_id_ = FlvTagHeader::AudioTag;
    audio_msg->timestamp_ = timestamp;

    AUDIODATA audio_data;
    audio_data.header.sound_rate = AudioTagHeader::KHZ_44;
    audio_data.header.sound_format = AudioTagHeader::AAC;
    audio_data.header.sound_size = AudioTagHeader::Sample_16bit;
    audio_data.header.aac_packet_type = AudioTagHeader::AACRaw;

    audio_data.payload = std::string_view((char *)audio_msg->get_using_data().data() + 2, aac_bytes_);
    memcpy((char *)audio_msg->get_using_data().data() + 2, aac_data_, aac_bytes_);
    auto ret = audio_data.encode((uint8_t *)audio_msg->get_using_data().data(), 2 + aac_bytes_);
    if (ret < 0) {
        spdlog::error("encode audio rtmp message failed, code:{}", ret);
    }
    audio_msg->inc_used_bytes(aac_bytes_ + 2);
    return audio_msg;
}

void WebRtcToRtmp::process_audio_packet(std::shared_ptr<RtpPacket> pkt) {
    if (audio_codec_ && audio_codec_->get_codec_type() == CODEC_OPUS) {
        process_opus_packet(pkt);
    }
}

void WebRtcToRtmp::process_opus_packet(std::shared_ptr<RtpPacket> pkt) {
    if (!swr_context_) {
        AVChannelLayout out_ch_layout;
        AVChannelLayout in_ch_layout;
        av_channel_layout_default(&out_ch_layout, 2);
        av_channel_layout_default(&in_ch_layout, 2);
        int ret = swr_alloc_set_opts2(&swr_context_, &out_ch_layout, AV_SAMPLE_FMT_S16, 44100, &in_ch_layout,
                                      AV_SAMPLE_FMT_S16, 48000, 0, NULL);
        if (ret < 0) {
            spdlog::error("swr init failed");
            return;
        }

        if (swr_init(swr_context_) < 0) {
            spdlog::error("swr init failed");
            return;
        }
    }

    if (!opus_decoder_) {
        spdlog::error("opus decoder not initialize");
        return;
    }

    int in_samples = opus_decode(opus_decoder_.get(), (uint8_t *)pkt->get_payload().data(),
                                 pkt->get_payload().size(), decoded_pcm_, 4096, 0);
    if (swr_context_) {
        // int dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_context_, 48000) + in_samples, 44100, 48000,
        // AV_ROUND_UP);
        uint8_t *data_in[1];
        uint8_t *data_out[1];
        data_in[0] = (uint8_t *)decoded_pcm_;
        data_out[0] = (uint8_t *)resampled_pcm_;
        int32_t total_samples = 0;
        int out_samples =
            swr_convert(swr_context_, (uint8_t **)&data_out, 1024, (const uint8_t **)&data_in, in_samples);
        total_samples += out_samples;
        data_out[0] = (uint8_t *)resampled_pcm_ + total_samples * 2 * 2;
        while ((out_samples = swr_convert(swr_context_, (uint8_t **)&data_out, 1024, NULL, 0)) > 0) {
            data_out[0] = (uint8_t *)resampled_pcm_ + total_samples * 2 * 2;
            total_samples += out_samples;
        }

        auto aac_bytes =
            aac_encoder_->encode((uint8_t *)resampled_pcm_, total_samples * 2 * 2, aac_data_, 8192);
        if (aac_bytes > 0) {
            aac_bytes_ = aac_bytes;
            auto audio_msg = generate_aac_rtmp_msg((pkt->get_timestamp() - first_rtp_audio_ts_) / 48);
            if (audio_msg) {
                rtmp_media_source_->on_audio_packet(audio_msg);
            }
        }
    }
}

void WebRtcToRtmp::close() {
    if (closed_.test_and_set(std::memory_order_acquire)) {
        return;
    }

    auto self(shared_from_this());
    boost::asio::co_spawn(
        worker_->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            check_closable_timer_.cancel();
            co_await wg_.wait();

            if (rtmp_media_source_) {
                rtmp_media_source_->close();
                rtmp_media_source_ = nullptr;
            }

            auto origin_source = origin_source_.lock();
            if (rtp_media_sink_) {
                rtp_media_sink_->set_source_codec_ready_cb({});
                rtp_media_sink_->set_video_pkts_cb({});
                rtp_media_sink_->set_audio_pkts_cb({});

                rtp_media_sink_->close();
                if (origin_source) {
                    origin_source->remove_media_sink(rtp_media_sink_);
                }
                rtp_media_sink_ = nullptr;
            }

            if (origin_source) {
                origin_source->remove_bridge(shared_from_this());
            }
            co_return;
        },
        boost::asio::detached);
}
