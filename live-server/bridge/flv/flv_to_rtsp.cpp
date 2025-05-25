#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>

#include "spdlog/spdlog.h"
#include "flv_to_rtsp.hpp"

#include "base/thread/thread_worker.hpp"
#include "core/rtsp_media_source.hpp"
#include "core/flv_media_sink.hpp"

#include "protocol/rtmp/flv/flv_define.hpp"
#include "protocol/rtmp/flv/flv_tag.hpp"
#include "protocol/sdp/sdp.hpp"
#include "protocol/rtmp/rtmp_message/data_message/rtmp_metadata_message.hpp"

#include "codec/h264/h264_codec.hpp"
#include "codec/hevc/hevc_codec.hpp"
#include "codec/aac/aac_codec.hpp"
#include "codec/aac/mpeg4_aac.hpp"
#include "codec/mp3/mp3_codec.hpp"

#include "codec/aac/adts.hpp"
#include "base/utils/utils.h"
#include "config/app_config.h"
#include "app/publish_app.h"

using namespace mms;

FlvToRtsp::FlvToRtsp(ThreadWorker *worker, std::shared_ptr<PublishApp> app, std::weak_ptr<MediaSource> origin_source, 
                     const std::string & domain_name, const std::string & app_name, const std::string & stream_name) : 
                     MediaBridge(worker, app, origin_source, domain_name, app_name, stream_name), 
                     check_closable_timer_(worker->get_io_context()),
                     wg_(worker) {
    source_ = std::make_shared<RtspMediaSource>(worker, std::weak_ptr<StreamSession>(std::shared_ptr<StreamSession>(nullptr)), publish_app_);
    rtsp_media_source_ = std::static_pointer_cast<RtspMediaSource>(source_);
    sink_ = std::make_shared<FlvMediaSink>(worker); 
    flv_media_sink_ = std::static_pointer_cast<FlvMediaSink>(sink_); 
    spdlog::info("create FlvToRtsp");
}

FlvToRtsp::~FlvToRtsp() {
    spdlog::debug("destroy FlvToRtsp");
}

bool FlvToRtsp::init() {
    auto self(shared_from_this());
    wg_.add(1);
    boost::asio::co_spawn(worker_->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        boost::system::error_code ec;
        auto app_conf = publish_app_->get_conf();
        while (1) {
            check_closable_timer_.expires_from_now(std::chrono::milliseconds(app_conf->bridge_config().no_players_timeout_ms()/2));//30s检查一次
            co_await check_closable_timer_.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            if (boost::asio::error::operation_aborted == ec) {
                break;
            }

            if (rtsp_media_source_->has_no_sinks_for_time(app_conf->bridge_config().no_players_timeout_ms())) {//已经30秒没人播放了
                spdlog::debug("close FlvToRtsp because no players for 30s");
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
        rtsp_media_source_->set_status(status);
        if (status == E_SOURCE_STATUS_OK) {
            flv_media_sink_->on_flv_tag([this, self](std::vector<std::shared_ptr<FlvTag>> & flv_tags)->boost::asio::awaitable<bool> {
                for (auto flv_tag : flv_tags) {
                    auto tag_type = flv_tag->get_tag_type();
                    if (tag_type == FlvTagHeader::AudioTag) {
                        if (!co_await this->on_audio_packet(flv_tag)) {
                            co_return false;
                        }
                    } else if (tag_type == FlvTagHeader::VideoTag) {
                        if (!co_await this->on_video_packet(flv_tag)) {
                            co_return false;
                        }
                    } else if (tag_type == FlvTagHeader::ScriptTag) {
                        if (!co_await this->on_metadata(flv_tag)) {
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


boost::asio::awaitable<bool> FlvToRtsp::on_metadata(std::shared_ptr<FlvTag> metadata_pkt) {
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
        } else if (video_codec_id == VideoTagHeader::HEVC) {
            video_codec_ = std::make_shared<HevcCodec>();
        } else {
            co_return false;
        }
    }

    if (has_audio_) {
        auto audio_codec_id = metadata_->get_audio_codec_id();
        if (audio_codec_id == AudioTagHeader::AAC) {
            audio_codec_ = std::make_shared<AACCodec>();
        }  else if (audio_codec_id == AudioTagHeader::MP3) {
            audio_codec_ = std::make_shared<MP3Codec>();
        } else {
            co_return false;
        }
    }

    co_return true;
} 

boost::asio::awaitable<bool> FlvToRtsp::on_video_packet(std::shared_ptr<FlvTag> video_pkt) {
    spdlog::info("on_video_packet hevc");
    auto payload = video_pkt->get_using_data();
    payload.remove_prefix(FLV_TAG_HEADER_BYTES);
    if (!video_codec_) {
        VideoTagHeader header;
        header.decode((uint8_t*)payload.data(), payload.size());
        if (header.get_codec_id() == VideoTagHeader::AVC) {
            video_codec_ = std::make_shared<H264Codec>();
        } else if (header.get_codec_id() == VideoTagHeader::HEVC || header.get_codec_id() == VideoTagHeader::HEVC_FOURCC) {
            video_codec_ = std::make_shared<HevcCodec>();
        } else {
            co_return false;
        }
    }

    if (video_codec_->get_codec_type() == CODEC_H264) {
        co_return co_await process_h264_packet(video_pkt);
    } else if (video_codec_->get_codec_type() == CODEC_HEVC) {
        co_return co_await process_h265_packet(video_pkt);
    }
    
    co_return true;
}

boost::asio::awaitable<bool> FlvToRtsp::on_audio_packet(std::shared_ptr<FlvTag> audio_pkt) {
    if (!audio_codec_) {
        co_return false;
    }

    if (audio_codec_->get_codec_type() == CODEC_AAC) {
        co_return co_await process_aac_packet(audio_pkt);
    } 
    co_return true;
}

boost::asio::awaitable<bool> FlvToRtsp::process_h264_packet(std::shared_ptr<FlvTag> video_pkt) {
    VideoTagHeader header;
    std::string_view payload = video_pkt->get_using_data();
    payload.remove_prefix(FLV_TAG_HEADER_BYTES);
    int32_t header_consumed = header.decode((uint8_t*)payload.data(), payload.size());
    if (header_consumed < 0) {
        co_return false;
    }

    H264Codec *h264_codec = ((H264Codec*)video_codec_.get());
    if (header.is_seq_header()) {// 关键帧索引
        video_ready_ = true;
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

        if (!sdp_) {
            generate_sdp();
        }

        co_return true;
    } 

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
    
    VIDEODATA *video_data = (VIDEODATA *)video_pkt->tag_data.get();
    if (first_video_pts_ == 0) {
        first_video_pts_ = video_pkt->tag_header.timestamp + video_data->header.composition_time;
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

    auto rtp_pkts = video_rtp_packer_.pack(nalus, video_pt_, 0, (video_pkt->tag_header.timestamp + video_data->header.composition_time - first_video_pts_)*90);
    co_await rtsp_media_source_->on_video_packets(rtp_pkts);
    co_return true;
}

boost::asio::awaitable<bool> FlvToRtsp::process_h265_packet(std::shared_ptr<FlvTag> video_pkt) {
    VideoTagHeader header;
    std::string_view payload = video_pkt->get_using_data();
    payload.remove_prefix(FLV_TAG_HEADER_BYTES);
    int32_t header_consumed = header.decode((uint8_t*)payload.data(), payload.size());
    if (header_consumed < 0) {
        co_return false;
    }

    HevcCodec *hevc_codec = ((HevcCodec*)video_codec_.get());
    if (header.is_seq_header()) {// 关键帧索引
        video_ready_ = true;
        video_header_ = video_pkt;
        // 解析hevc configuration heade
        HEVCDecoderConfigurationRecord hevc_decoder_configuration_record;
        int32_t consumed = hevc_decoder_configuration_record.decode((uint8_t*)payload.data() + header_consumed, payload.size() - header_consumed);
        if (consumed < 0) {
            co_return false;
        }

        hevc_codec->set_sps_pps_vps(hevc_decoder_configuration_record.get_sps(), 
                                    hevc_decoder_configuration_record.get_pps(), 
                                    hevc_decoder_configuration_record.get_vps());
        nalu_length_size_ = hevc_decoder_configuration_record.lengthSizeMinusOne + 1;
        if (nalu_length_size_ == 3 || nalu_length_size_ > 4) {
            co_return false;
        }

        if (!sdp_) {
            generate_sdp();
        }

        co_return true;
    } 

    // 获取到nalus
    std::list<std::string_view> nalus;
    auto consumed = get_nalus((uint8_t*)payload.data() + header_consumed, payload.size() - header_consumed, nalus);
    if (consumed < 0) {
        co_return false;
    }

    // 判断sps,pps,aud等
    bool has_sps_nalu = false;
    bool has_pps_nalu = false;
    bool has_vps_nalu = false;
    bool has_key_nalu = false;
    if (!video_codec_) {
        co_return false;
    }
    
    VIDEODATA *video_data = (VIDEODATA *)video_pkt->tag_data.get();
    if (first_video_pts_ == 0) {
        first_video_pts_ = video_pkt->tag_header.timestamp + video_data->header.composition_time;
    }

    for (auto it = nalus.begin(); it != nalus.end(); it++) {
        HEVC_NALU_TYPE nalu_type = H265_TYPE(*it->data());
        if (nalu_type == NAL_VPS) {
            has_vps_nalu = true;
            hevc_codec->set_vps(std::string((it)->data(), (it)->size()));
        } else if (nalu_type == NAL_SPS) {
            has_sps_nalu = true;
            hevc_codec->set_sps(std::string((it)->data(), (it)->size()));
        } else if (nalu_type == NAL_PPS) {
            has_pps_nalu = true;
            hevc_codec->set_pps(std::string((it)->data(), (it)->size()));
        } else if (nalu_type >= NAL_BLA_W_LP && nalu_type <= NAL_RSV_IRAP_VCL23) {
            has_key_nalu = true;
        }
    }

    if (has_key_nalu) {
        if (!has_pps_nalu) {
            nalus.insert(nalus.begin(), std::string_view(hevc_codec->get_pps_nalu().data(), hevc_codec->get_pps_nalu().size()));
        }

        if (!has_sps_nalu) {
            nalus.insert(nalus.begin(), std::string_view(hevc_codec->get_sps_nalu().data(), hevc_codec->get_sps_nalu().size()));
        }

        if (!has_vps_nalu) {
            nalus.insert(nalus.begin(), std::string_view(hevc_codec->get_vps_nalu().data(), hevc_codec->get_sps_nalu().size()));
        }
    }

    // static uint8_t default_aud_nalu[] = { 0x09, 0xf0 };
    // static std::string_view aud_nalu((char*)default_aud_nalu, 2);
    // if (!has_aud_nalu) {
    //     nalus.push_front(aud_nalu);
    // }

    auto rtp_pkts = video_rtp_packer_.pack(nalus, video_pt_, 0, (video_pkt->tag_header.timestamp + video_data->header.composition_time - first_video_pts_)*90);
    co_await rtsp_media_source_->on_video_packets(rtp_pkts);
    co_return true;
}

int32_t FlvToRtsp::get_nalus(uint8_t *data, int32_t len, std::list<std::string_view> & nalus) {
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

boost::asio::awaitable<bool> FlvToRtsp::process_aac_packet(std::shared_ptr<FlvTag> audio_pkt) {
    AudioTagHeader header;
    std::string_view payload = audio_pkt->get_using_data();
    payload.remove_prefix(FLV_TAG_HEADER_BYTES);
    int32_t header_consumed = header.decode((uint8_t*)payload.data(), payload.size());
    if (header_consumed < 0) {
        co_return false;
    }
    
    AACCodec *aac_codec = ((AACCodec*)audio_codec_.get());    
    if (header.is_seq_header()) {// 关键帧索引
        audio_ready_ = true;
        audio_header_ = audio_pkt;
        // 解析avc configuration heade
        auto audio_config = std::make_shared<AudioSpecificConfig>();
        int32_t consumed = audio_config->parse((uint8_t*)payload.data() + header_consumed, payload.size() - header_consumed);
        if (consumed < 0) {
            spdlog::error("parse aac audio header failed, ret:{}", consumed);
            co_return false;
        }

        aac_codec->set_audio_specific_config(audio_config);
        if (!sdp_) {
            generate_sdp();
        }
        co_return true;
    }

    if (!audio_ready_) {
        co_return false;
    }
    // 切分为AU
    auto audio_config = aac_codec->get_audio_specific_config();
    if (!audio_config) {
        co_return false;
    }

    int32_t payload_size = payload.size() - header_consumed;
    int16_t au_header_bytes = (13 + 3)/8;
    char *p = (char*)audio_buf_;
    *(uint16_t*)p = htons(au_header_bytes*8);
    p += 2;
    memset(p, 0, 2);   
    BitStream bit_stream(std::string_view(p, au_header_bytes));
    int16_t au_size = payload_size;
    bit_stream.write_bits(13, au_size);
    int32_t au_index = 0;
    bit_stream.write_bits(3, au_index);

    p += 2;
    memcpy(p, payload.data() + header_consumed, payload_size);
    p += payload_size;

    auto rtp_pkts = audio_rtp_packer_.pack((char*)audio_buf_, p - (char*)audio_buf_, audio_pt_, 0, audio_pkt->tag_header.timestamp*audio_config->sampling_frequency/1000);
    auto ret = co_await rtsp_media_source_->on_audio_packets(rtp_pkts);
    if (!ret) {
        co_return false;
    }
    co_return true;
}

/*
o=- 0 0 IN IP4 127.0.0.1
s=No Name
c=IN IP4 192.168.172.134
t=0 0
a=tool:libavformat 59.34.102
m=video 0 RTP/AVP 96
b=AS:1890
a=rtpmap:96 H264/90000
a=fmtp:96 packetization-mode=1; sprop-parameter-sets=Z2QAHqzZQFAFuwEQAGXTsBMS0AjxYtlg,aOrgjLIs; profile-level-id=64001E
a=control:streamid=0
m=audio 0 RTP/AVP 97
b=AS:128
a=rtpmap:97 MPEG4-GENERIC/48000/2
a=fmtp:97 profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3; config=119056E500
a=control:streamid=1
*/
/*
o=- 0 0 IN IP4 127.0.0.1
s=test.publish.com/app/test
t=0 0
a=tool:mms
m=video 0 RTP/AVP 96
a=rtpmap:96 H264/90000
a=fmtp:96 profile-level-id=64001E; sprop-parameter-sets=Z2QAHqzZQFAFuwEQAGXTsBMS0AjxYtlg,aOrgjLIs; packetization-mode=1
a=control:streamid=0
m=audio 0 RTP/AVP 97
a=rtpmap:97 MPEG4-GENERIC/44100/2
a=fmtp:97 config=121056E500; indexdeltalength=3; indexlength=3; sizelength=13; mode=AAC-hbr; profile-level-id=1
a=control:streamid=1
*/

/*
o=- 12312374342672830711 1 IN IP4 127.0.0.1
s=test.publish.com/app/test
t=0 0
a=tool:mms
m=audio 0 RTP/AVP/TCP 97
a=rtpmap:97 MPEG4-GENERIC/44100/2
a=fmtp:97 config=1210; indexdeltalength=3; indexlength=3; sizelength=13; mode=AAC-hbr; profile-level-id=1
a=control:streamid=0
m=video 0 RTP/AVP/TCP 96
a=rtpmap:96 H264/90000
a=fmtp:96 sprop-parameter-sets=Z0LAHtoCgL/lwFqAgICgAAADACAAAAZR4sXU,aM48gA==; profile-level-id=42001f; level-asymmetry-allowed=1; packetization-mode=1
a=control:streamid=1
*/
bool FlvToRtsp::generate_sdp() {
    if (has_audio_ && !audio_codec_->is_ready()) {
        spdlog::debug("audio not ready");
        return false;
    }

    if (has_video_ && !video_codec_->is_ready()) {
        spdlog::debug("video not ready");
        return false;
    }

    sdp_ = std::make_shared<Sdp>();
    sdp_->set_version(0);
    sdp_->set_origin({"-", 0, 0, "IN", "IP4", "127.0.0.1"}); // o=- get_rand64 1 IN IP4 127.0.0.1
    auto session_name = domain_name_ + "/" + app_name_ + "/" + stream_name_;
    sdp_->set_session_name(session_name);                                  //
    sdp_->set_time({0, 0});                                                // t=0 0
    sdp_->set_tool({"mms"});
    if (has_audio_) {
        MediaSdp audio_sdp;
        audio_sdp.set_media("audio");
        audio_sdp.set_port(0);
        audio_sdp.set_port_count(1);
        audio_sdp.set_proto("RTP/AVP");
        audio_sdp.add_fmt(audio_pt_);
        audio_sdp.set_control(Control("streamid=1"));
        if (audio_codec_->get_codec_type() == CODEC_AAC) {
            auto aac_codec = (AACCodec*)audio_codec_.get();
            auto audio_specific_config = aac_codec->get_audio_specific_config();
            if (!audio_specific_config) {
                return false;
            }
            Payload audio_payload(audio_pt_, "MPEG4-GENERIC", audio_specific_config->sampling_frequency, {std::to_string(audio_specific_config->channel_configuration)});
            Fmtp fmtp;
            /*
            struct AUConfig {
                uint32_t constant_size;//constant_size和size_length不能同时出现, 一般都是size_length，目前先处理这种情况，constant_size应该是在某种编码模式固定长度的时候用
                uint16_t size_length = 0;//The number of bits on which the AU-size field is encoded in the AU-header. 如果没有这个字段，则au-headers-length字段也不存在
                uint16_t index_length = 0;//The number of bits on which the AU-Index is encoded in the first AU-header
                uint16_t index_delta_length = 0;//The number of bits on which the AU-Index-delta field is encoded in any non-first AU-header.
                uint16_t cts_delta_length;//The number of bits on which the CTS-delta field is encoded in the AU-header.
                uint16_t dts_delta_length;//The number of bits on which the DTS-delta field is encoded in the AU-header.
                uint8_t random_access_indication;//A decimal value of zero or one, indicating whether the RAP-flag is present in the AU-header.
                uint8_t stream_state_indication;//The number of bits on which the Stream-state field is encoded in the AU-header.
                uint16_t auxiliary_data_size_length;//The number of bits that is used to encode the auxiliary-data-size field.
            };
            */
            fmtp.set_pt(audio_pt_);
            fmtp.add_param("profile-level-id", "1");
            fmtp.add_param("mode", "AAC-hbr");
            fmtp.add_param("sizelength", "13");
            fmtp.add_param("indexlength", "3");
            fmtp.add_param("indexdeltalength", "3");
            auto config_size = audio_specific_config->size();
            std::string config;
            config.resize(config_size);
            audio_specific_config->encode((uint8_t*)config.data(), config_size);
            std::string hex_config;
            Utils::bin_to_hex_str(config, hex_config);
            fmtp.add_param("config", hex_config);

            audio_payload.add_fmtp(fmtp);
            audio_sdp.add_payload(audio_payload);
            sdp_->add_media_sdp(audio_sdp);
        }
    }

    if (has_video_) {
        MediaSdp video_sdp;
        video_sdp.set_media("video");
        video_sdp.set_port(0);
        video_sdp.set_port_count(1);
        video_sdp.set_proto("RTP/AVP");
        video_sdp.set_control(Control("streamid=0"));
        if (video_codec_->get_codec_type() == CODEC_H264) {
            auto h264_codec = (H264Codec*)video_codec_.get();
            auto sps = h264_codec->get_sps_nalu();
            auto pps = h264_codec->get_pps_nalu();
            std::string sps_base64;
            std::string pps_base64;
            Utils::encode_base64(sps, sps_base64);
            Utils::encode_base64(pps, pps_base64);
            video_sdp.add_fmt(video_pt_);
            Payload video_payload(video_pt_, "H264", 90000, {});
            Fmtp fmtp;
            fmtp.set_pt(video_pt_);
            fmtp.add_param("packetization-mode", "1");
            fmtp.add_param("level-asymmetry-allowed", "1");
            fmtp.add_param("profile-level-id", "42001f");
            fmtp.add_param("sprop-parameter-sets", sps_base64 + ","+pps_base64);
            video_payload.add_fmtp(fmtp);

            video_sdp.add_payload(video_payload);
            sdp_->add_media_sdp(video_sdp);
        } else if (video_codec_->get_codec_type() == CODEC_HEVC) {
            auto h265_codec = (HevcCodec*)video_codec_.get();
            auto sps = h265_codec->get_sps_nalu();
            auto pps = h265_codec->get_pps_nalu();
            auto vps = h265_codec->get_vps_nalu();
            std::string sps_base64;
            std::string pps_base64;
            std::string vps_base64;
            Utils::encode_base64(sps, sps_base64);
            Utils::encode_base64(pps, pps_base64);
            Utils::encode_base64(vps, vps_base64);
            video_sdp.add_fmt(video_pt_);
            Payload video_payload(video_pt_, "H265", 90000, {});
            Fmtp fmtp;
            fmtp.set_pt(video_pt_);
            fmtp.add_param("packetization-mode", "1");
            fmtp.add_param("level-asymmetry-allowed", "1");
            fmtp.add_param("profile-level-id", "42001f");
            fmtp.add_param("sprop-vps", vps_base64);
            fmtp.add_param("sprop-sps", sps_base64);
            fmtp.add_param("sprop-pps", pps_base64);
            video_payload.add_fmtp(fmtp);

            video_sdp.add_payload(video_payload);
            sdp_->add_media_sdp(video_sdp);
        }
    }
    rtsp_media_source_->set_video_pt(video_pt_);
    rtsp_media_source_->set_audio_pt(audio_pt_);
    rtsp_media_source_->set_announce_sdp(sdp_);
    spdlog::info("generate sdp:{}", sdp_->to_string());
    return true;
}

void FlvToRtsp::close() {
    if (closed_.test_and_set(std::memory_order_acquire)) {
        return;
    }

    auto self(shared_from_this());
    boost::asio::co_spawn(worker_->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        check_closable_timer_.cancel();
        co_await wg_.wait();

        if (rtsp_media_source_) {
            rtsp_media_source_->close();
            rtsp_media_source_ = nullptr;
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