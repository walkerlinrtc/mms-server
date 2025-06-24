#include "rtsp_to_flv.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>

#include "app/publish_app.h"
#include "base/thread/thread_worker.hpp"
#include "base/wait_group.h"
#include "codec/aac/aac_codec.hpp"
#include "codec/codec.hpp"
#include "codec/h264/h264_codec.hpp"
#include "codec/hevc/hevc_codec.hpp"
#include "codec/hevc/hevc_define.hpp"
#include "config/app_config.h"
#include "core/flv_media_source.hpp"
#include "core/rtp_media_sink.hpp"
#include "protocol/rtp/h265_rtp_pkt_info.h"


using namespace mms;

RtspToFlv::RtspToFlv(ThreadWorker *worker, std::shared_ptr<PublishApp> app,
                     std::weak_ptr<MediaSource> origin_source, const std::string &domain_name,
                     const std::string &app_name, const std::string &stream_name)
    : MediaBridge(worker, app, origin_source, domain_name, app_name, stream_name),
      check_closable_timer_(worker->get_io_context()),
      wg_(worker) {
    source_ = std::make_shared<FlvMediaSource>(
        worker, std::weak_ptr<StreamSession>(std::shared_ptr<StreamSession>(nullptr)), app);
    flv_media_source_ = std::static_pointer_cast<FlvMediaSource>(source_);
    sink_ = std::make_shared<RtpMediaSink>(worker);
    rtp_media_sink_ = std::static_pointer_cast<RtpMediaSink>(sink_);
    video_frame_cache_ = std::make_unique<char[]>(1024 * 1024);
    audio_frame_cache_ = std::make_unique<char[]>(1024 * 20);
    type_ = "rtsp-to-flv";
}

RtspToFlv::~RtspToFlv() {}

bool RtspToFlv::init() {
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

                if (flv_media_source_->has_no_sinks_for_time(
                        app_conf->bridge_config().no_players_timeout_ms())) {  // 已经30秒没人播放了
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
    
    rtp_media_sink_->on_close([this, self]() {
        close();
    });

    rtp_media_sink_->set_source_codec_ready_cb(
        [this, self](std::shared_ptr<Codec> video_codec, std::shared_ptr<Codec> audio_codec) -> bool {
            video_codec_ = video_codec;
            audio_codec_ = audio_codec;
            if (video_codec_) {
                has_video_ = true;
            }

            if (audio_codec_) {
                if (audio_codec_->get_codec_type() != CODEC_AAC) {
                    return false;
                }
                has_audio_ = true;
            }

            if (!generate_metadata()) {
                return false;
            }

            if (!flv_media_source_->on_metadata(metadata_pkt_)) {
                return false;
            }

            if (!generate_video_header()) {
                return false;
            }

            if (!flv_media_source_->on_video_packet(video_header_)) {
                return false;
            }

            if (!generate_audio_header()) {
                return false;
            }

            if (!flv_media_source_->on_audio_packet(audio_header_)) {
                return false;
            }

            return true;
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

bool RtspToFlv::generate_metadata() {
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
    if (audio_codec_->get_codec_type() == CODEC_AAC) {
        auto aac_codec = std::static_pointer_cast<AACCodec>(audio_codec_);
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
        metadata_amf0_.set_item_value("audiodatarate", (double)audio_codec_->get_data_rate());
        metadata_amf0_.set_item_value("audiosamplesize", 16.0);  // 好像aac是固定的16bit
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
        if (h264_codec->get_fps(video_fps_)) {
            metadata_amf0_.set_item_value("framerate", video_fps_);
        }
        metadata_amf0_.set_item_value("videodatarate", (double)video_codec_->get_data_rate());
    } else if (video_codec_->get_codec_type() == CODEC_HEVC) {
        auto hevc_codec = std::static_pointer_cast<HevcCodec>(video_codec_);
        metadata_amf0_.set_item_value("videocodecid", 12.0);
        uint32_t w, h;
        hevc_codec->get_wh(w, h);
        metadata_amf0_.set_item_value("width", double(w));
        metadata_amf0_.set_item_value("height", double(h));
        if (hevc_codec->get_fps(video_fps_)) {
            metadata_amf0_.set_item_value("framerate", video_fps_);
        }
        metadata_amf0_.set_item_value("videodatarate", (double)video_codec_->get_data_rate());
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

bool RtspToFlv::generate_video_header() {
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
    } else if (video_codec_->get_codec_type() == CODEC_HEVC) {
        auto hevc_codec = std::static_pointer_cast<HevcCodec>(video_codec_);
        auto &decode_configuration_record = hevc_codec->get_hevc_configuration();
        auto payload_size = decode_configuration_record.size();
        video_header_ = std::make_shared<FlvTag>(FLV_TAG_HEADER_BYTES + 5 +
                                                 payload_size);  // 5字节是video tag header的大小
        // rtmp头
        video_header_->tag_header.stream_id = 0;
        video_header_->tag_header.tag_type = FlvTagHeader::VideoTag;
        video_header_->tag_header.timestamp = 0;
        // flv video tag header
        auto video_data = std::make_unique<VIDEODATA>();
        auto payload = video_header_->get_unuse_data();
        payload.remove_prefix(FLV_TAG_HEADER_BYTES + 5);
        auto ret = decode_configuration_record.encode((uint8_t *)payload.data(), payload.size());
        if (ret < 0) {
        } else {
        }

        video_data->payload = payload;
        video_data->header.frame_type = VideoTagHeader::KeyFrame;
        video_data->header.is_extheader = true;
        video_data->header.fourcc = VideoTagHeader::HEVC_FOURCC;
        video_data->header.ext_packet_type = VideoTagHeader::PacketTypeSequeneStart;
        video_data->header.composition_time = 0;
        video_header_->tag_data = std::move(video_data);
        video_header_->tag_header.data_size = 5 + payload_size;
        video_header_->encode();
        return true;
    }

    return false;
}

bool RtspToFlv::generate_audio_header() {
    audio_header_ = std::make_shared<FlvTag>();
    if (audio_codec_->get_codec_type() == CODEC_AAC) {
        auto aac_codec = std::static_pointer_cast<AACCodec>(audio_codec_);
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
        audio_header_ =
            std::make_shared<FlvTag>(FLV_TAG_HEADER_BYTES + 2 +
                                     payload_size);  // 11字节flv tag头+2字节aac头+audio_specific_config字节数
        audio_header_->tag_header.tag_type = FlvTagHeader::AudioTag;
        audio_header_->tag_header.data_size = 2 + payload_size;
        audio_header_->tag_header.stream_id = 0;
        audio_header_->tag_header.timestamp = 0;

        auto payload = audio_header_->get_unuse_data();
        payload.remove_prefix(FLV_TAG_HEADER_BYTES + 2);
        auto ret = audio_config->encode((uint8_t *)payload.data(), payload.size());
        if (ret < 0) {
            return false;
        }

        audio_data->payload = payload;
        audio_header_->tag_data = std::move(audio_data);
        audio_header_->encode();
        return true;
    }

    return false;
}

void RtspToFlv::process_video_packet(std::shared_ptr<RtpPacket> pkt) {
    if (video_codec_ && video_codec_->get_codec_type() == CODEC_H264) {
        process_h264_packet(pkt);
    } else if (video_codec_ && video_codec_->get_codec_type() == CODEC_HEVC) {
        process_h265_packet(pkt);
    }
}

void RtspToFlv::process_h264_packet(std::shared_ptr<RtpPacket> pkt) {
    auto h264_nalu = rtp_h264_depacketizer_.on_packet(pkt);
    if (h264_nalu) {
        uint32_t this_timestamp = h264_nalu->get_timestamp();
        auto flv_tag = generate_h264_flv_tag((this_timestamp - first_rtp_video_ts_) / 90,
                                             h264_nalu);  // todo 除90这个要根据sdp来计算，目前固定
        if (flv_tag) {
            flv_media_source_->on_video_packet(flv_tag);
        }
    }
}

std::shared_ptr<FlvTag> RtspToFlv::generate_h264_flv_tag(uint32_t timestamp,
                                                         std::shared_ptr<RtpH264NALU> &nalu) {
    bool is_key = false;
    char *buf = video_frame_cache_.get();
    auto &pkts = nalu->get_rtp_pkts();
    int32_t total_payload_bytes = 0;
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
                total_payload_bytes = buf - video_frame_cache_.get();
                std::shared_ptr<FlvTag> video_flv_tag =
                    std::make_shared<FlvTag>(FLV_TAG_HEADER_BYTES + 5 + total_payload_bytes);
                video_flv_tag->tag_header.tag_type = FlvTagHeader::VideoTag;
                video_flv_tag->tag_header.data_size = total_payload_bytes + 5;
                video_flv_tag->tag_header.stream_id = 0;
                video_flv_tag->tag_header.timestamp = timestamp;
                auto video_data = std::make_unique<VIDEODATA>();

                video_data->header.frame_type =
                    is_key ? VideoTagHeader::KeyFrame : VideoTagHeader::InterFrame;
                video_data->header.codec_id = VideoTagHeader::AVC;
                video_data->header.avc_packet_type = VideoTagHeader::AVCNALU;
                if (is_key) {
                    video_dts_ = timestamp;
                    video_data->header.composition_time = 0;  // rtp默认不支持b帧先
                } else {
                    video_dts_ += 1000 / video_fps_;
                    video_data->header.composition_time = timestamp - video_dts_;
                }
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
            }

            uint32_t s = htonl(pkt->get_payload().size());
            memcpy(buf, &s, 4);
            buf += 4;
            memcpy(buf, pkt->get_payload().data(), pkt->get_payload().size());
            buf += pkt->get_payload().size();

            if (pkt->get_header().marker == 1) {
                total_payload_bytes = buf - video_frame_cache_.get();
                std::shared_ptr<FlvTag> video_flv_tag =
                    std::make_shared<FlvTag>(FLV_TAG_HEADER_BYTES + 5 + total_payload_bytes);
                video_flv_tag->tag_header.tag_type = FlvTagHeader::VideoTag;
                video_flv_tag->tag_header.data_size = total_payload_bytes + 5;
                video_flv_tag->tag_header.stream_id = 0;
                video_flv_tag->tag_header.timestamp = timestamp;

                auto video_data = std::make_unique<VIDEODATA>();
                video_data->header.frame_type =
                    is_key ? VideoTagHeader::KeyFrame : VideoTagHeader::InterFrame;
                video_data->header.codec_id = VideoTagHeader::AVC;
                video_data->header.avc_packet_type = VideoTagHeader::AVCNALU;
                if (is_key) {
                    video_dts_ = timestamp;
                    video_data->header.composition_time = 0;  // rtp默认不支持b帧先
                } else {
                    video_dts_ += 1000 / video_fps_;
                    video_data->header.composition_time = timestamp - video_dts_;
                }
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
                total_payload_bytes = buf - video_frame_cache_.get();
                std::shared_ptr<FlvTag> video_flv_tag =
                    std::make_shared<FlvTag>(FLV_TAG_HEADER_BYTES + 5 + total_payload_bytes);
                video_flv_tag->tag_header.data_size = total_payload_bytes + 5;
                video_flv_tag->tag_header.stream_id = 0;
                video_flv_tag->tag_header.tag_type = FlvTagHeader::VideoTag;
                video_flv_tag->tag_header.timestamp = timestamp;

                auto video_data = std::make_unique<VIDEODATA>();
                video_data->header.frame_type =
                    is_key ? VideoTagHeader::KeyFrame : VideoTagHeader::InterFrame;
                video_data->header.codec_id = VideoTagHeader::AVC;
                video_data->header.avc_packet_type = VideoTagHeader::AVCNALU;
                if (is_key) {
                    video_dts_ = timestamp;
                    video_data->header.composition_time = 0;  // rtp默认不支持b帧先
                } else {
                    video_dts_ += 1000 / video_fps_;
                    video_data->header.composition_time = timestamp - video_dts_;
                }
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

void RtspToFlv::process_h265_packet(std::shared_ptr<RtpPacket> pkt) {
    auto h265_nalu = rtp_h265_depacketizer_.on_packet(pkt);
    if (h265_nalu) {
        uint32_t this_timestamp = h265_nalu->get_timestamp();
        auto flv_tag = generate_h265_flv_tag((this_timestamp - first_rtp_video_ts_) / 90,
                                             h265_nalu);  // todo 除90这个要根据sdp来计算，目前固定
        if (flv_tag) {
            flv_media_source_->on_video_packet(flv_tag);
        }
    }
}

std::shared_ptr<FlvTag> RtspToFlv::generate_h265_flv_tag(uint32_t timestamp,
                                                         std::shared_ptr<RtpH265NALU> &nalu) {
    bool is_key = false;
    char *buf = video_frame_cache_.get();
    auto &pkts = nalu->get_rtp_pkts();
    int32_t total_payload_bytes = 0;
    for (auto it = pkts.begin(); it != pkts.end(); it++) {
        auto pkt = it->second;

        H265RtpPktInfo pkt_info;
        pkt_info.parse(pkt->get_payload().data(), pkt->get_payload().size());

        if (pkt_info.is_single_nalu()) {
            if (pkt_info.get_nalu_type() >= NAL_BLA_W_LP && pkt_info.get_nalu_type() <= NAL_RSV_IRAP_VCL23) {
                is_key = true;
            }

            uint32_t s = htonl(pkt->get_payload().size());
            memcpy(buf, &s, 4);
            buf += 4;
            memcpy(buf, pkt->get_payload().data(), pkt->get_payload().size());
            buf += pkt->get_payload().size();

            if (pkt->get_header().marker == 1) {
                total_payload_bytes = buf - video_frame_cache_.get();
                std::shared_ptr<FlvTag> video_flv_tag =
                    std::make_shared<FlvTag>(FLV_TAG_HEADER_BYTES + 8 + total_payload_bytes);
                video_flv_tag->tag_header.tag_type = FlvTagHeader::VideoTag;
                video_flv_tag->tag_header.stream_id = 0;
                video_flv_tag->tag_header.timestamp = timestamp;

                auto video_data = std::make_unique<VIDEODATA>();
                video_data->header.is_extheader = true;
                video_data->header.fourcc = VideoTagHeader::HEVC_FOURCC;
                video_data->header.codec_id = VideoTagHeader::HEVC;
                video_data->header.avc_packet_type = VideoTagHeader::AVCNALU;
                video_data->header.frame_type =
                    is_key ? VideoTagHeader::KeyFrame : VideoTagHeader::InterFrame;
                video_data->header.ext_packet_type = VideoTagHeader::PacketTypeCodedFrames;
                if (is_key) {
                    video_dts_ = timestamp;
                    video_data->header.composition_time = 0;  // rtp默认不支持b帧先
                } else {
                    video_dts_ += 1000 / video_fps_;
                    video_data->header.composition_time = timestamp - video_dts_;
                }
                video_data->payload = std::string_view(video_frame_cache_.get(), total_payload_bytes);
                video_flv_tag->tag_header.data_size = total_payload_bytes + video_data->header.size();
                video_flv_tag->tag_data = std::move(video_data);
                auto ret = video_flv_tag->encode();
                if (ret < 0) {
                    spdlog::error("encode flv video tag failed, code:{}", ret);
                }
                return video_flv_tag;
            }
        } else if (pkt_info.is_fu()) {
            int32_t nalu_size = 0;
            int32_t *nalu_size_buf_pos = (int32_t *)buf;
            buf += 4;
            if (pkt_info.is_start_fu()) {
                do {
                    if (pkt_info.is_start_fu()) {
                        if (pkt_info.get_nalu_type() >= NAL_BLA_W_LP &&
                            pkt_info.get_nalu_type() <= NAL_RSV_IRAP_VCL23) {
                            is_key = true;
                        }

                        const uint8_t *pkt_buf = (const uint8_t *)pkt->get_payload().data();
                        uint8_t p0 = *pkt_buf;
                        uint8_t p1 = *(pkt_buf + 1);
                        uint8_t fu_header = pkt_buf[2];      // FU header
                        uint8_t fu_type = fu_header & 0x3F;  // 提取低6位FuType

                        buf[0] = (p0 & 0x81) | (fu_type << 1);  // 假设Type占6位，调整掩码和移位
                        buf[1] = p1;
                        memcpy(buf + 2, pkt->get_payload().data() + 3, pkt->get_payload().size() - 3);
                        buf += pkt->get_payload().size() - 1;
                        nalu_size += pkt->get_payload().size() - 1;
                    } else {
                        memcpy(buf, pkt->get_payload().data() + 3, pkt->get_payload().size() - 3);
                        buf += pkt->get_payload().size() - 3;
                        nalu_size += pkt->get_payload().size() - 3;
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
                } while (pkt_info.is_fu() && it != pkts.end());
            }

            if (pkt->get_header().marker == 1) {
                total_payload_bytes = buf - video_frame_cache_.get();
                std::shared_ptr<FlvTag> video_flv_tag =
                    std::make_shared<FlvTag>(FLV_TAG_HEADER_BYTES + 8 + total_payload_bytes);
                video_flv_tag->tag_header.stream_id = 0;
                video_flv_tag->tag_header.tag_type = FlvTagHeader::VideoTag;
                video_flv_tag->tag_header.timestamp = timestamp;

                auto video_data = std::make_unique<VIDEODATA>();
                video_data->header.is_extheader = true;
                video_data->header.fourcc = VideoTagHeader::HEVC_FOURCC;
                video_data->header.codec_id = VideoTagHeader::HEVC;
                video_data->header.avc_packet_type = VideoTagHeader::AVCNALU;
                video_data->header.frame_type =
                    is_key ? VideoTagHeader::KeyFrame : VideoTagHeader::InterFrame;
                video_data->header.ext_packet_type = VideoTagHeader::PacketTypeCodedFrames;
                if (is_key) {
                    video_dts_ = timestamp;
                    video_data->header.composition_time = 0;  // rtp默认不支持b帧先
                } else {
                    video_dts_ += 1000 / video_fps_;
                    video_data->header.composition_time = timestamp - video_dts_;
                }
                video_data->payload = std::string_view(video_frame_cache_.get(), total_payload_bytes);
                video_flv_tag->tag_header.data_size = total_payload_bytes + video_data->header.size();
                video_flv_tag->tag_data = std::move(video_data);

                auto ret = video_flv_tag->encode();
                if (ret < 0) {
                    spdlog::error("encode flv video tag failed, code:{}", ret);
                } else {
                }
                return video_flv_tag;
            }
        }
    }
    return nullptr;
}

std::shared_ptr<FlvTag> RtspToFlv::generate_aac_flv_tag(uint32_t timestamp,
                                                        std::shared_ptr<RtpAACNALU> &nalu) {
    /*
        2.4.  Fragmentation of Access Units

        MPEG allows for very large Access Units.  Since most IP networks have
        significantly smaller MTU sizes, this payload format allows for the
        fragmentation of an Access Unit over multiple RTP packets.  Hence,
        when an IP packet is lost after IP-level fragmentation, only an AU
        fragment may get lost instead of the entire AU.  To simplify the
        implementation of RTP receivers, an RTP packet SHALL either carry one
        or more complete Access Units or a single fragment of one AU, i.e.,
        packets MUST NOT contain fragments of multiple Access Units.
    */
    char *buf = audio_frame_cache_.get();
    auto &pkts = nalu->get_rtp_pkts();
    std::shared_ptr<AACCodec> aac_codec = std::static_pointer_cast<AACCodec>(audio_codec_);
    auto au_config = aac_codec->get_au_config();
    if (!au_config) {
        return nullptr;
    }

    int16_t au_per_header_bytes = (au_config->size_length + au_config->index_length + 7) / 8;
    // spdlog::info("au_config->size_length:{}, au_config->index_length:{}", au_config->size_length,
    // au_config->index_length);
    for (auto it = pkts.begin(); it != pkts.end(); it++) {
        auto pkt = it->second;
        const uint8_t *pkt_buf = (const uint8_t *)pkt->get_payload().data();
        int32_t left_bytes = pkt->get_payload().size();
        const uint8_t *au_section_buf;
        if (au_config->size_length != 0) {  // 有sizelength参数
            uint16_t au_header_bits = ntohs(*(uint16_t *)pkt_buf);
            if (au_header_bits % 8 != 0) {
                return nullptr;
            }
            size_t au_header_bytes_total = au_header_bits / 8;
            int16_t au_header_count = au_header_bytes_total / au_per_header_bytes;

            // 检查头部长度的完整性
            if (size_t(au_header_count * au_per_header_bytes) != au_header_bytes_total) {
                return nullptr;
            }

            BitStream bit_stream(std::string_view((char *)pkt_buf + 2, au_header_bytes_total));
            au_section_buf = pkt_buf + 2 + au_header_bytes_total;
            left_bytes -= 2 + au_header_bytes_total;
            for (auto i = 0; i < au_header_count; i++) {
                uint16_t au_size = 0;
                if (!bit_stream.read_u(au_config->size_length, au_size)) {
                    return nullptr;
                }

                if (au_size == 0) {
                    return nullptr;
                }

                uint16_t au_index = 0;
                if (!bit_stream.read_u(au_config->index_length, au_index)) {
                    return nullptr;
                }

                int32_t copy_bytes = au_size;
                if (copy_bytes >= left_bytes) {
                    copy_bytes = left_bytes;
                }
                memcpy(buf, au_section_buf, copy_bytes);
                buf += copy_bytes;
                au_section_buf += copy_bytes;
            }

            if (pkt->get_header().marker == 1) {
                int32_t total_payload_bytes = buf - audio_frame_cache_.get();
                std::shared_ptr<FlvTag> audio_flv_tag =
                    std::make_shared<FlvTag>(FLV_TAG_HEADER_BYTES + 2 + total_payload_bytes);
                audio_flv_tag->tag_header.tag_type = FlvTagHeader::AudioTag;
                audio_flv_tag->tag_header.data_size = total_payload_bytes + 2;
                audio_flv_tag->tag_header.stream_id = 0;
                audio_flv_tag->tag_header.timestamp = timestamp;

                auto audio_data = std::make_unique<AUDIODATA>();
                auto audio_config = aac_codec->get_audio_specific_config();
                if (!audio_config) {
                    return nullptr;
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
                audio_data->header.sound_format = AudioTagHeader::AAC;
                audio_data->header.sound_size = AudioTagHeader::Sample_16bit;
                audio_data->header.aac_packet_type = AudioTagHeader::AACRaw;

                audio_data->payload = std::string_view((char *)audio_frame_cache_.get(), total_payload_bytes);
                audio_flv_tag->tag_data = std::move(audio_data);

                auto ret = audio_flv_tag->encode();
                if (ret < 0) {
                    spdlog::error("encode audio flv tag failed, code:{}", ret);
                }
                return audio_flv_tag;
            }
        }
    }
    return nullptr;
}

void RtspToFlv::process_audio_packet(std::shared_ptr<RtpPacket> pkt) {
    if (audio_codec_ && audio_codec_->get_codec_type() == CODEC_AAC) {
        process_aac_packet(pkt);
    }
}

void RtspToFlv::process_aac_packet(std::shared_ptr<RtpPacket> pkt) {
    auto aac_nalu = rtp_aac_depacketizer_.on_packet(pkt);
    if (aac_nalu) {
        uint32_t this_timestamp = aac_nalu->get_timestamp();
        auto aac_codec = std::static_pointer_cast<AACCodec>(audio_codec_);
        auto audio_config = aac_codec->get_audio_specific_config();
        if (!audio_config || audio_config->sampling_frequency <= 0) {
            return;
        }

        auto flv_tag = generate_aac_flv_tag(
            ((this_timestamp - first_rtp_audio_ts_) * 1000) / audio_config->sampling_frequency,
            aac_nalu);  // todo 除90这个要根据sdp来计算，目前固定
        if (flv_tag) {
            flv_media_source_->on_audio_packet(flv_tag);
        }
    }
}

void RtspToFlv::close() {
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
                rtp_media_sink_->set_video_pkts_cb({});
                rtp_media_sink_->set_audio_pkts_cb({});
                rtp_media_sink_->set_source_codec_ready_cb({});
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
