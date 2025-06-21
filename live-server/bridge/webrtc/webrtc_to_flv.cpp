#include "webrtc_to_flv.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/system/error_code.hpp>

#include "app/publish_app.h"
#include "base/thread/thread_worker.hpp"
#include "codec/aac/aac_codec.hpp"
#include "codec/aac/aac_encoder.hpp"
#include "codec/codec.hpp"
#include "codec/h264/h264_codec.hpp"
#include "codec/hevc/hevc_codec.hpp"
#include "codec/opus/opus_codec.hpp"
#include "config/app_config.h"
#include "core/flv_media_source.hpp"
#include "core/rtp_media_sink.hpp"
#include "protocol/rtmp/flv/flv_define.hpp"

using namespace mms;

WebRtcToFlv::WebRtcToFlv(ThreadWorker *worker, std::shared_ptr<PublishApp> app, std::weak_ptr<MediaSource> origin_source, const std::string &domain_name, const std::string &app_name,
                         const std::string &stream_name)
    : MediaBridge(worker, app, origin_source, domain_name, app_name, stream_name), 
    check_closable_timer_(worker->get_io_context()), wg_(worker) {
    source_ = std::make_shared<FlvMediaSource>(worker, std::weak_ptr<StreamSession>(std::shared_ptr<StreamSession>(nullptr)), publish_app_);
    flv_media_source_ = std::static_pointer_cast<FlvMediaSource>(source_);
    sink_ = std::make_shared<RtpMediaSink>(worker);
    rtp_media_sink_ = std::static_pointer_cast<RtpMediaSink>(sink_);
    video_frame_cache_ = std::make_unique<char[]>(1024 * 1024);
    audio_frame_cache_ = std::make_unique<char[]>(1024 * 20);
    type_ = "webrtc-to-flv";
}

WebRtcToFlv::~WebRtcToFlv() { 
    if (swr_context_) {
        swr_free(&swr_context_);
        swr_context_ = nullptr;
    }
    spdlog::debug("destroy WebRtcToFlv"); 
}

bool WebRtcToFlv::init() {
    auto self(shared_from_this());
    wg_.add(1);
    boost::asio::co_spawn(
        worker_->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            boost::system::error_code ec;
            auto app_conf = publish_app_->get_conf();
            while (1) {
                check_closable_timer_.expires_after(std::chrono::milliseconds(app_conf->bridge_config().no_players_timeout_ms())); // 30s检查一次
                co_await check_closable_timer_.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                if (boost::asio::error::operation_aborted == ec) {
                    break;
                }

                if (flv_media_source_->has_no_sinks_for_time(app_conf->bridge_config().no_players_timeout_ms())) { // 已经30秒没人播放了
                    spdlog::debug("close WebRtcToFlv because no players for 30s");
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

    rtp_media_sink_->on_close([this, self]() { close(); });

    rtp_media_sink_->set_source_codec_ready_cb([this, self](std::shared_ptr<Codec> video_codec, std::shared_ptr<Codec> audio_codec) -> bool {
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
                int ret = swr_alloc_set_opts2(&swr_context_, &out_ch_layout, AV_SAMPLE_FMT_S16, 44100, &in_ch_layout, AV_SAMPLE_FMT_S16, opus_codec->getFs(), 0, NULL);
                if (ret < 0) {
                    spdlog::error("swr init failed");
                    return false;
                }
            }
            if (swr_init(swr_context_) < 0) {
                spdlog::error("swr init failed");
                return false;
            }

            has_audio_ = true;
            aac_encoder_ = std::make_unique<AACEncoder>();
            aac_encoder_->init(44100, opus_codec->get_channels());
            my_audio_codec_ = std::make_shared<AACCodec>();
            auto spec_conf = aac_encoder_->get_specific_configuration();
            std::shared_ptr<AudioSpecificConfig> audio_specific_config = std::make_shared<AudioSpecificConfig>();
            auto ret = audio_specific_config->parse((uint8_t*)spec_conf.data(), spec_conf.size());
            if (ret > 0) {
                my_audio_codec_->set_audio_specific_config(audio_specific_config);
            }
        }

        if (video_codec_ && !video_codec_->is_ready()) {
            spdlog::error("vcodec is not ready");
            return true;
        }

        if (audio_codec_ && !audio_codec_->is_ready()) {
            spdlog::error("acodec is not ready");
            return true;
        }

        return generateFlvHeaders();
    });

    rtp_media_sink_->set_on_source_status_changed_cb([this, self](SourceStatus status) -> boost::asio::awaitable<void> {
        flv_media_source_->set_status(status);
        if (status == E_SOURCE_STATUS_OK) {
            on_status_ok();
        }
        co_return;
    });

    return true;
}

void WebRtcToFlv::on_status_ok() {
    auto self(shared_from_this());
    rtp_media_sink_->set_video_pkts_cb([this, self](std::vector<std::shared_ptr<RtpPacket>> pkts) -> boost::asio::awaitable<bool> {
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

    rtp_media_sink_->set_audio_pkts_cb([this, self](std::vector<std::shared_ptr<RtpPacket>> pkts) -> boost::asio::awaitable<bool> {
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
}

bool WebRtcToFlv::generateFlvHeaders() {
    if (!generate_metadata()) {
        return false;
    }

    if (!generate_video_header()) {
        return false;
    }

    if (!generate_audio_header()) {
        return false;
    }

    if (!flv_media_source_->on_metadata(metadata_pkt_)) {
        return false;
    }

    if (!flv_media_source_->on_video_packet(video_header_)) {
        return false;
    }

    if (!flv_media_source_->on_audio_packet(audio_header_)) {
        return false;
    }

    header_ready_ = true;
    return true;
}

bool WebRtcToFlv::generate_metadata() {
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
    metadata_amf0_.set_item_value("audiocodecid", 10.0); // aac
    // 固定值
    if (my_audio_codec_->get_codec_type() == CODEC_AAC) {
        auto aac_codec = std::static_pointer_cast<AACCodec>(my_audio_codec_);
        metadata_amf0_.set_item_value("audiocodecid", 10.0);
        auto audio_specific_config = aac_codec->get_audio_specific_config();
        if (!audio_specific_config) {
            return false;
        }
        metadata_amf0_.set_item_value("audiochannels", (double)audio_specific_config->channel_configuration);
        if (audio_specific_config->channel_configuration >= 2) {
            metadata_amf0_.set_item_value("stereo", true);
        } else {
            metadata_amf0_.set_item_value("stereo", false);
        }
        metadata_amf0_.set_item_value("audiodatarate", (double)my_audio_codec_->get_data_rate());
        metadata_amf0_.set_item_value("audiosamplesize", 16.0); // 好像aac是固定的16bit
    } else {
        return false;
    }

    metadata_amf0_.set_item_value("duration", 0.0);
    metadata_amf0_.set_item_value("encoder", "mms");
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
    } else {
        return false;
    }

    auto total_bytes = name1.size() + name2.size() + metadata_amf0_.size();
    metadata_pkt_ = std::make_shared<FlvTag>(FLV_TAG_HEADER_BYTES + total_bytes);
    metadata_pkt_->tag_header.tag_type = FlvTagHeader::ScriptTag;
    metadata_pkt_->tag_header.stream_id = 0;
    metadata_pkt_->tag_header.timestamp = 0;
    metadata_pkt_->tag_header.data_size = total_bytes;

    auto unuse_data = metadata_pkt_->get_unuse_data();
    uint8_t *payload = (uint8_t *)unuse_data.data() + FLV_TAG_HEADER_BYTES;
    size_t payload_bytes = unuse_data.size() - FLV_TAG_HEADER_BYTES;
    int32_t consumed1 = name1.encode(payload, payload_bytes);
    int32_t consumed2 = name2.encode(payload + consumed1, payload_bytes - consumed1);
    metadata_amf0_.encode(payload + consumed1 + consumed2, payload_bytes - consumed1 - consumed2);

    auto script_data = std::make_unique<SCRIPTDATA>();
    script_data->payload = std::string_view((char *)payload, total_bytes);
    metadata_pkt_->tag_data = std::move(script_data);
    metadata_pkt_->encode();
    return true;
}

bool WebRtcToFlv::generate_video_header() {
    video_header_ = std::make_shared<FlvTag>();
    video_header_->tag_header.data_size = 0;
    video_header_->tag_header.stream_id = 0;
    video_header_->tag_header.tag_type = FlvTagHeader::VideoTag;
    video_header_->tag_header.timestamp = 0;

    auto video_data = std::make_unique<VIDEODATA>();
    if (video_codec_->get_codec_type() == CODEC_H264) {
        auto h264_codec = std::static_pointer_cast<H264Codec>(video_codec_);
        AVCDecoderConfigurationRecord &decode_configuration_record = h264_codec->get_avc_configuration();
        auto payload_size = decode_configuration_record.size();
        // 5 video tag header
        video_header_ = std::make_shared<FlvTag>(FLV_TAG_HEADER_BYTES + 5 + payload_size);
        video_header_->tag_header.stream_id = 0;
        video_header_->tag_header.tag_type = FlvTagHeader::VideoTag;
        video_header_->tag_header.timestamp = 0;

        auto video_data = std::make_unique<VIDEODATA>();
        video_data->header.frame_type = VideoTagHeader::KeyFrame;
        video_data->header.codec_id = VideoTagHeader::AVC;
        video_data->header.avc_packet_type = VideoTagHeader::AVCSequenceHeader;
        video_data->header.composition_time = 0;

        video_header_->tag_header.data_size = 5 + payload_size;
        auto payload = video_header_->get_unuse_data();
        payload.remove_prefix(FLV_TAG_HEADER_BYTES + 5);
        auto ret = decode_configuration_record.encode((uint8_t *)payload.data(), payload.size());
        if (ret < 0) {
        } else {
        }

        video_data->payload = payload;
        video_header_->tag_data = std::move(video_data);
        int32_t consumed = video_header_->encode();

        auto using_data = video_header_->get_using_data();
        using_data.remove_prefix(16);
        if (consumed < 0) {
            spdlog::error("video header encode failed, consumed:{}", consumed);
            return false;
        }
        return true;
    }

    return false;
}

bool WebRtcToFlv::generate_audio_header() {
    audio_header_ = std::make_shared<FlvTag>();
    if (my_audio_codec_->get_codec_type() == CODEC_AAC) {
        auto aac_codec = std::static_pointer_cast<AACCodec>(my_audio_codec_);
        auto audio_data = std::make_unique<AUDIODATA>();
        audio_data->header.sound_format = AudioTagHeader::AAC;
        auto audio_config = aac_codec->get_audio_specific_config();
        if (!audio_config) {
            return false;
        }

        if (audio_config->sampling_frequency == 44100) {
            audio_data->header.sound_rate = AudioTagHeader::KHZ_44;
        } else if (audio_config->sampling_frequency == 11025) {
            audio_data->header.sound_rate = AudioTagHeader::KHZ_11;
        } else if (audio_config->sampling_frequency == 22050) {
            audio_data->header.sound_rate = AudioTagHeader::KHZ_22;
        } else {
            audio_data->header.sound_rate = AudioTagHeader::KHZ_44;
        }

        audio_data->header.sound_size = AudioTagHeader::Sample_16bit;
        audio_data->header.aac_packet_type = AudioTagHeader::AACSequenceHeader;

        auto payload_size = audio_config->size();
        audio_header_ = std::make_shared<FlvTag>(FLV_TAG_HEADER_BYTES + 2 + payload_size); // 11字节flv tag头+2字节aac头+audio_specific_config字节数
        audio_header_->tag_header.tag_type = FlvTagHeader::AudioTag;
        audio_header_->tag_header.data_size = 2 + payload_size;
        audio_header_->tag_header.stream_id = 0;
        audio_header_->tag_header.timestamp = 0;

        auto payload = audio_header_->get_unuse_data();
        payload.remove_prefix(FLV_TAG_HEADER_BYTES + 2);
        uint8_t* write_ptr = (uint8_t*)payload.data();
        memset(write_ptr, 0, payload_size);
        auto ret = audio_config->encode(write_ptr, payload_size);
        if (ret < 0) {
            return false;
        }
        audio_data->payload = payload;
        audio_header_->tag_data = std::move(audio_data);
        audio_header_->encode();
        return true;
    }
    return true;
}

void WebRtcToFlv::process_video_packet(std::shared_ptr<RtpPacket> pkt) {
    if (video_codec_ && video_codec_->get_codec_type() == CODEC_H264) {
        process_h264_packet(pkt);
    }
}

void WebRtcToFlv::process_h264_packet(std::shared_ptr<RtpPacket> pkt) {
    auto h264_nalu = rtp_h264_depacketizer_.on_packet(pkt);
    if (h264_nalu) {
        uint32_t this_timestamp = h264_nalu->get_timestamp();
        auto video_tag = generate_h264_flv_tag((this_timestamp - first_rtp_video_ts_) / 90, h264_nalu); // todo 除90这个要根据sdp来计算，目前固定
        if (!video_header_ && video_codec_->is_ready()) {
            H264Codec* h264_codec = (H264Codec*)video_codec_.get();
            uint32_t w, h;
            h264_codec->get_wh(w, h);
        }

        if (!stream_ready_) {
            stream_ready_ = (has_audio_?(audio_codec_ && audio_codec_->is_ready()):true) && (has_video_?(video_codec_ && video_codec_->is_ready()):true);
            if (stream_ready_) {
                generateFlvHeaders();
            }
        }

        if (video_tag && header_ready_) {
            flv_media_source_->on_video_packet(video_tag);
        }
    }
}

std::shared_ptr<FlvTag> WebRtcToFlv::generate_h264_flv_tag(uint32_t timestamp, std::shared_ptr<RtpH264NALU> &nalu) {
    bool is_key = false;
    char *buf = video_frame_cache_.get();
    auto &pkts = nalu->get_rtp_pkts();
    int32_t total_payload_bytes = 0;
    H264Codec* h264_codec = (H264Codec*)video_codec_.get();
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
                } else if (nalu_type == H264NaluTypeSPS) {
                    h264_codec->set_sps(std::string((char*)data+2, nalu_size));
                } else if (nalu_type == H264NaluTypePPS) {
                    h264_codec->set_pps(std::string((char*)data+2, nalu_size));
                }

                uint32_t s = htonl(nalu_size);
                memcpy(buf, &s, 4);
                buf += 4;
                memcpy(buf, data + 2, nalu_size);
                buf += nalu_size;

                pos += 2 + nalu_size;
                data += 2 + nalu_size;
            }

            if (pkt->get_header().marker == 1) { // 最后一个
                total_payload_bytes = buf - video_frame_cache_.get();
                std::shared_ptr<FlvTag> video_flv_tag = std::make_shared<FlvTag>(FLV_TAG_HEADER_BYTES + 5 + total_payload_bytes);
                video_flv_tag->tag_header.tag_type = FlvTagHeader::VideoTag;
                video_flv_tag->tag_header.data_size = total_payload_bytes + 5;
                video_flv_tag->tag_header.stream_id = 0;
                video_flv_tag->tag_header.timestamp = timestamp;
                auto video_data = std::make_unique<VIDEODATA>();

                video_data->header.frame_type = is_key ? VideoTagHeader::KeyFrame : VideoTagHeader::InterFrame;
                video_data->header.codec_id = VideoTagHeader::AVC;
                video_data->header.avc_packet_type = VideoTagHeader::AVCNALU;
                video_data->header.composition_time = 0;
                video_data->payload = std::string_view(video_frame_cache_.get(), total_payload_bytes);

                video_flv_tag->tag_data = std::move(video_data);

                auto ret = video_flv_tag->encode();
                if (ret < 0) {
                    // spdlog::error("encode flv tag failed, code:{}", ret);
                }
                return video_flv_tag;
            }
        } else if (pkt_info.is_single_nalu()) {
            if (pkt_info.get_nalu_type() == H264NaluTypeIDR) {
                is_key = true;
            } else if (pkt_info.get_nalu_type() == H264NaluTypeSPS) {
                h264_codec->set_sps(std::string(pkt->get_payload()));
            } else if (pkt_info.get_nalu_type() == H264NaluTypePPS) {
                h264_codec->set_pps(std::string(pkt->get_payload()));
            }

            uint32_t s = htonl(pkt->get_payload().size());
            memcpy(buf, &s, 4);
            buf += 4;
            memcpy(buf, pkt->get_payload().data(), pkt->get_payload().size());
            buf += pkt->get_payload().size();

            if (pkt->get_header().marker == 1) {
                total_payload_bytes = buf - video_frame_cache_.get();
                std::shared_ptr<FlvTag> video_flv_tag = std::make_shared<FlvTag>(FLV_TAG_HEADER_BYTES + 5 + total_payload_bytes);
                video_flv_tag->tag_header.tag_type = FlvTagHeader::VideoTag;
                video_flv_tag->tag_header.data_size = total_payload_bytes + 5;
                video_flv_tag->tag_header.stream_id = 0;
                video_flv_tag->tag_header.timestamp = timestamp;

                auto video_data = std::make_unique<VIDEODATA>();
                video_data->header.frame_type = is_key ? VideoTagHeader::KeyFrame : VideoTagHeader::InterFrame;
                video_data->header.codec_id = VideoTagHeader::AVC;
                video_data->header.avc_packet_type = VideoTagHeader::AVCNALU;
                video_data->header.composition_time = 0;
                video_data->payload = std::string_view(video_frame_cache_.get(), total_payload_bytes);

                video_flv_tag->tag_data = std::move(video_data);
                auto ret = video_flv_tag->encode();
                if (ret < 0) {
                    spdlog::error("encode flv video tag failed, code:{}", ret);
                }
                return video_flv_tag;
            }
        } else if (pkt_info.get_type() == H264_RTP_PAYLOAD_FU_A) {
            int32_t nalu_size = 0;
            int32_t *nalu_size_buf_pos = (int32_t *)buf;
            buf += 4;
            if (pkt_info.is_start_fu()) {
                do {
                    if (pkt_info.is_start_fu()) {
                        bool is_sps = false;
                        bool is_pps = false;
                        if (pkt_info.get_nalu_type() == H264NaluTypeIDR) {
                            is_key = true;
                        } else if (pkt_info.get_nalu_type() == H264NaluTypeSPS) {
                            is_sps = true;
                        } else if (pkt_info.get_nalu_type() == H264NaluTypePPS) {
                            is_pps = true;
                        }

                        nalu_size += pkt->get_payload().size() - 1;
                        const uint8_t *pkt_buf = (const uint8_t *)pkt->get_payload().data();
                        uint8_t nalu_type = (pkt_buf[0] & 0xe0) | (pkt_buf[1] & 0x1F);
                        char * buf_start = buf;
                        memcpy(buf, &nalu_type, 1);
                        memcpy(buf + 1, pkt->get_payload().data() + 2, pkt->get_payload().size() - 2);
                        buf += pkt->get_payload().size() - 1;

                        if (is_sps) {
                            h264_codec->set_sps(std::string(buf_start, buf - buf_start));
                        } else if (is_pps) {
                            h264_codec->set_pps(std::string(buf_start, buf - buf_start));
                        }
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
                total_payload_bytes = buf - video_frame_cache_.get();
                std::shared_ptr<FlvTag> video_flv_tag = std::make_shared<FlvTag>(FLV_TAG_HEADER_BYTES + 5 + total_payload_bytes);
                video_flv_tag->tag_header.data_size = total_payload_bytes + 5;
                video_flv_tag->tag_header.stream_id = 0;
                video_flv_tag->tag_header.tag_type = FlvTagHeader::VideoTag;
                video_flv_tag->tag_header.timestamp = timestamp;

                auto video_data = std::make_unique<VIDEODATA>();
                video_data->header.frame_type = is_key ? VideoTagHeader::KeyFrame : VideoTagHeader::InterFrame;
                video_data->header.codec_id = VideoTagHeader::AVC;
                video_data->header.avc_packet_type = VideoTagHeader::AVCNALU;
                video_data->header.composition_time = 0;
                video_data->payload = std::string_view((char *)video_frame_cache_.get(), total_payload_bytes);

                video_flv_tag->tag_data = std::move(video_data);

                auto ret = video_flv_tag->encode();
                if (ret < 0) {
                    spdlog::error("encode flv video tag failed, code:{}", ret);
                }
                return video_flv_tag;
            }
        }
    }
    return nullptr;
}

std::shared_ptr<FlvTag> WebRtcToFlv::generate_aac_flv_tag(uint32_t timestamp) {
    std::shared_ptr<FlvTag> audio_flv_tag = std::make_shared<FlvTag>(FLV_TAG_HEADER_BYTES + 2 + aac_bytes_);
    audio_flv_tag->tag_header.data_size = aac_bytes_ + 2;
    audio_flv_tag->tag_header.stream_id = 0;
    audio_flv_tag->tag_header.tag_type = FlvTagHeader::AudioTag;
    audio_flv_tag->tag_header.timestamp = timestamp;

    auto audio_data = std::make_unique<AUDIODATA>();
    audio_data->header.sound_rate = AudioTagHeader::KHZ_44;
    audio_data->header.sound_format = AudioTagHeader::AAC;
    audio_data->header.sound_size = AudioTagHeader::Sample_16bit;
    audio_data->header.aac_packet_type = AudioTagHeader::AACRaw;

    audio_data->payload = std::string_view((char *)aac_data_, aac_bytes_);
    audio_flv_tag->tag_data = std::move(audio_data);

    auto ret = audio_flv_tag->encode();
    if (ret < 0) {
        spdlog::error("encode audio flv tag failed, code:{}", ret);
    }
    return audio_flv_tag;
}

void WebRtcToFlv::process_audio_packet(std::shared_ptr<RtpPacket> pkt) {
    if (audio_codec_ && audio_codec_->get_codec_type() == CODEC_OPUS) {
        process_opus_packet(pkt);
    }
}

void WebRtcToFlv::process_opus_packet(std::shared_ptr<RtpPacket> pkt) {
    if (!swr_context_) {
        AVChannelLayout out_ch_layout;
        AVChannelLayout in_ch_layout;
        av_channel_layout_default(&out_ch_layout, 2);
        av_channel_layout_default(&in_ch_layout, 2);
        int ret = swr_alloc_set_opts2(&swr_context_, &out_ch_layout, AV_SAMPLE_FMT_S16, 44100, &in_ch_layout, AV_SAMPLE_FMT_S16, 48000, 0, NULL);
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

    int in_samples = opus_decode(opus_decoder_.get(), (uint8_t *)pkt->get_payload().data(), pkt->get_payload().size(), decoded_pcm_, 4096, 0);
    if (swr_context_) {
        // int dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_context_, 48000) + in_samples, 44100, 48000,
        // AV_ROUND_UP);
        uint8_t *data_in[1];
        uint8_t *data_out[1];
        data_in[0] = (uint8_t *)decoded_pcm_;
        data_out[0] = (uint8_t *)resampled_pcm_;
        int32_t total_samples = 0;
        int out_samples = swr_convert(swr_context_, (uint8_t **)&data_out, 1024, (const uint8_t **)&data_in, in_samples);
        total_samples += out_samples;
        data_out[0] = (uint8_t *)resampled_pcm_ + total_samples * 2 * 2;
        while ((out_samples = swr_convert(swr_context_, (uint8_t **)&data_out, 1024, NULL, 0)) > 0) {
            data_out[0] = (uint8_t *)resampled_pcm_ + total_samples * 2 * 2;
            total_samples += out_samples;
        }

        if (!aac_encoder_) {
            return;
        }

        auto aac_bytes = aac_encoder_->encode((uint8_t *)resampled_pcm_, total_samples * 2 * 2, aac_data_, 8192);
        if (aac_bytes > 0) {
            aac_bytes_ = aac_bytes;
            auto audio_flv_tag = generate_aac_flv_tag((pkt->get_timestamp() - first_rtp_audio_ts_) / 48);
            if (!stream_ready_) {
                stream_ready_ = (has_audio_?(audio_codec_ && my_audio_codec_->is_ready()):true) && (has_video_?(video_codec_ && video_codec_->is_ready()):true);
                if (stream_ready_) {
                    generateFlvHeaders();
                }
            }
            
            if (audio_flv_tag && header_ready_) {
                flv_media_source_->on_audio_packet(audio_flv_tag);
            }
        }
    }
}

void WebRtcToFlv::close() {
    if (closed_.test_and_set(std::memory_order_acquire)) {
        return;
    }

    auto self(shared_from_this());
    boost::asio::co_spawn(
        worker_->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            check_closable_timer_.cancel();
            co_await wg_.wait();

            if (flv_media_source_) {
                flv_media_source_->close();
                flv_media_source_ = nullptr;
            }

            auto origin_source = origin_source_.lock();
            if (rtp_media_sink_) {
                rtp_media_sink_->on_close({});
                rtp_media_sink_->set_source_codec_ready_cb({});
                rtp_media_sink_->set_on_source_status_changed_cb({});
                rtp_media_sink_->set_audio_pkts_cb({});
                rtp_media_sink_->set_video_pkts_cb({});

                rtp_media_sink_->close();
                if (origin_source) {
                    origin_source->remove_media_sink(rtp_media_sink_);
                }
                rtp_media_sink_ = nullptr;
            }

            if (origin_source) {
                origin_source->remove_bridge(self);
            }
            co_return;
        },
        boost::asio::detached);
}
