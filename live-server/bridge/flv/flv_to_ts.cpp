/*
 * @Author: jbl19860422
 * @Date: 2023-11-09 20:49:39
 * @LastEditTime: 2023-12-27 21:57:32
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\server\transcode\flv_to_ts.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>

#include "spdlog/spdlog.h"
#include "flv_to_ts.hpp"
#include "protocol/rtmp/flv/flv_define.hpp"
#include "protocol/rtmp/flv/flv_tag.hpp"
#include "protocol/ts/ts_pat_pmt.hpp"
#include "protocol/ts/ts_header.hpp"
#include "protocol/ts/ts_segment.hpp"

#include "protocol/rtmp/rtmp_message/data_message/rtmp_metadata_message.hpp"

#include "codec/h264/h264_codec.hpp"
#include "codec/hevc/hevc_codec.hpp"
#include "codec/av1/av1_codec.hpp"
#include "codec/aac/aac_codec.hpp"
#include "codec/aac/mpeg4_aac.hpp"
#include "codec/mp3/mp3_codec.hpp"
#include "codec/aac/adts.hpp"

#include "base/utils/utils.h"
#include "app/publish_app.h"
#include "core/flv_media_sink.hpp"
#include "protocol/rtmp/flv/flv_tag.hpp"
#include "config/app_config.h"

using namespace mms;
FlvToTs::FlvToTs(ThreadWorker *worker, std::shared_ptr<PublishApp> app, std::weak_ptr<MediaSource> origin_source, 
                 const std::string & domain_name, const std::string & app_name, const std::string & stream_name) : 
                 MediaBridge(worker, app, origin_source, domain_name, app_name, stream_name), 
                 check_closable_timer_(worker->get_io_context()),
                 wg_(worker) {
    sink_ = std::make_shared<FlvMediaSink>(worker);
    flv_media_sink_ = std::static_pointer_cast<FlvMediaSink>(sink_);
    source_ = std::make_shared<TsMediaSource>(worker, std::weak_ptr<StreamSession>(std::shared_ptr<StreamSession>(nullptr)), publish_app_);
    ts_media_source_ = std::static_pointer_cast<TsMediaSource>(source_);
    video_pes_segs_.reserve(1024);
    spdlog::info("create flv to ts");
}

FlvToTs::~FlvToTs() {

}

bool FlvToTs::init() {
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

            if (ts_media_source_->has_no_sinks_for_time(app_conf->bridge_config().no_players_timeout_ms())) {//已经30秒没人播放了
                spdlog::debug("close FlvToTs because no players for 30s");
                break;
            }
        }
        co_return;
    }, [this, self](std::exception_ptr exp) {
        (void)exp;
        wg_.done();
        close();
    });

    flv_media_sink_->on_flv_tag([this](const std::vector<std::shared_ptr<FlvTag>> & flv_tags)->boost::asio::awaitable<bool> {
        for (auto flv_tag : flv_tags) {
            auto tag_type = flv_tag->get_tag_type();
            if (tag_type == FlvTagHeader::AudioTag) {
                if (!this->on_audio_packet(flv_tag)) {
                    co_return false;
                }
            } else if (tag_type == FlvTagHeader::VideoTag) {
                if (!this->on_video_packet(flv_tag)) {
                    co_return false;
                }
            } else if (tag_type == FlvTagHeader::ScriptTag) {
                if (!this->on_metadata(flv_tag)) {
                    co_return false;
                }
            }
        }
        
        co_return true;
    });
    return true;
}

bool FlvToTs::on_metadata(std::shared_ptr<FlvTag> metadata_pkt) {
    metadata_ = std::make_shared<RtmpMetaDataMessage>();
    if (metadata_->decode(metadata_pkt) <= 0) {
        return false;
    }
    has_video_ = metadata_->has_video();
    has_audio_ = metadata_->has_audio();

    if (has_video_) {
        auto video_codec_id = metadata_->get_video_codec_id();
        if (video_codec_id == VideoTagHeader::AVC) {
            video_codec_ = std::make_shared<H264Codec>();
            video_pid_ = TS_VIDEO_AVC_PID;
            video_type_ = TsStreamVideoH264;
            continuity_counter_[video_pid_] = 0;
        } else if (video_codec_id == VideoTagHeader::HEVC || video_codec_id == VideoTagHeader::HEVC_FOURCC) {
            video_codec_ = std::make_shared<HevcCodec>();
            video_pid_ = TS_VIDEO_HEVC_PID;
            video_type_ = TsStreamVideoH265;
            continuity_counter_[video_pid_] = 0;
        } else {
            return false;
        }
    }

    if (has_audio_) {
        auto audio_codec_id = metadata_->get_audio_codec_id();
        if (audio_codec_id == AudioTagHeader::AAC) {
            audio_codec_ = std::make_shared<AACCodec>();
            audio_pid_ = TS_AUDIO_AAC_PID;
            audio_type_ = TsStreamAudioAAC;
            continuity_counter_[audio_pid_] = 0;
            adts_headers_.reserve(200);
        } else if (audio_codec_id == AudioTagHeader::MP3) {
            audio_codec_ = std::make_shared<MP3Codec>();
            audio_pid_ = TS_AUDIO_MP3_PID;
            audio_type_ = TsStreamAudioMp3;
            continuity_counter_[audio_pid_] = 0;
        } else {
            return false;
        }
    }

    if (has_video_) {
        PCR_PID = video_pid_; 
    } else if (has_audio_) {
        PCR_PID = audio_pid_;
    }

    return true;
}

bool FlvToTs::on_video_packet(std::shared_ptr<FlvTag> video_pkt) {
    if (!video_codec_) {
        VideoTagHeader header;
        auto payload = video_pkt->get_using_data();
        header.decode((uint8_t*)payload.data(), payload.size());
        if (header.get_codec_id() == VideoTagHeader::AVC) {
            video_codec_ = std::make_shared<H264Codec>();
            video_pid_ = TS_VIDEO_AVC_PID;
            video_type_ = TsStreamVideoH264;
            continuity_counter_[video_pid_] = 0;
        } else if (header.get_codec_id() == VideoTagHeader::HEVC || header.get_codec_id() == VideoTagHeader::HEVC_FOURCC) {
            video_codec_ = std::make_shared<HevcCodec>();
            video_pid_ = TS_VIDEO_HEVC_PID;
            video_type_ = TsStreamVideoH265;
            continuity_counter_[video_pid_] = 0;
        } else {
            return false;
        }
    }

    if (video_codec_->get_codec_type() == CODEC_H264) {
        return process_h264_packet(video_pkt);
    } else if (video_codec_->get_codec_type() == CODEC_HEVC) {
        return process_h265_packet(video_pkt);
    }
    
    return false;
}

bool FlvToTs::process_h264_packet(std::shared_ptr<FlvTag> video_pkt) {
    VideoTagHeader header;
    auto payload = video_pkt->get_using_data();
    payload.remove_prefix(FLV_TAG_HEADER_BYTES);
    int32_t header_consumed = header.decode((uint8_t*)payload.data(), payload.size());
    if (header_consumed < 0) {
        return false;
    }

    H264Codec *h264_codec = ((H264Codec*)video_codec_.get());
    if (header.is_seq_header()) {// 关键帧索引
        video_ready_ = true;
        video_header_ = video_pkt;
        // 解析avc configuration heade
        AVCDecoderConfigurationRecord avc_decoder_configuration_record;
        int32_t consumed = avc_decoder_configuration_record.parse((uint8_t*)payload.data() + header_consumed, payload.size() - header_consumed);
        if (consumed < 0) {
            return false;
        }

        h264_codec->set_sps_pps(avc_decoder_configuration_record.get_sps(), avc_decoder_configuration_record.get_pps());
        nalu_length_size_ = avc_decoder_configuration_record.nalu_length_size_minus_one + 1;
        if (nalu_length_size_ == 3 || nalu_length_size_ > 4) {
            return false;
        }
        return true;
    } 

    bool is_key = header.is_key_frame() && !header.is_seq_header();
    if (!curr_seg_ && !is_key) {//片段开始的帧，必须是关键帧
        return false;
    }

    auto pes_packet = std::make_shared<PESPacket>();// pes_packet;
    auto & pes_header = pes_packet->pes_header;
    memset((void*)&pes_header, 0, sizeof(pes_header));

    if (curr_seg_) {
        if (publish_app_->can_reap_ts(is_key, curr_seg_)) {
            HLS_INFO("session:{}, reap ts seq:{}, name:{}, bytes:{}k, dur:{} by video", 
                            get_session_name(), 
                            curr_seg_->get_seqno(),
                            curr_seg_->get_filename(),
                            curr_seg_->get_ts_bytes()/1024,
                            curr_seg_->get_duration()
            );
            curr_seg_->set_reaped();
            on_ts_segment(curr_seg_);
            curr_seg_ = nullptr;
        }
    }

    if (!curr_seg_) {
        curr_seg_ = std::make_shared<TsSegment>();
        std::string_view pat_seg = curr_seg_->alloc_ts_packet();
        create_pat(pat_seg);
        std::string_view pmt_seg = curr_seg_->alloc_ts_packet();
        create_pmt(pmt_seg);
    }
    // 获取到nalus
    std::list<std::string_view> nalus;
    auto consumed = get_nalus((uint8_t*)payload.data() + header_consumed, payload.size() - header_consumed, nalus);
    if (consumed < 0) {
        return false;
    }

    // 判断sps,pps,aud等
    bool has_aud_nalu = false;
    bool has_sps_nalu = false;
    bool has_pps_nalu = false;
    if (!video_codec_) {
        return false;
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
    // 计算payload长度(第一个nalu头部4个字节，后面的头部只需要3字节)
    int32_t payload_size = nalus.size()*3 + 1;//头部的总字节数
    for (auto & nalu : nalus) {
        payload_size += nalu.size();
    }

    // 添加上pes header
    // uint8_t *pes = video_pes_.get();
    pes_header.stream_id = TsPESStreamIdVideoCommon;

    char *pes = video_pes_header_;
    static char pes_start_prefix[3] = {0x00, 0x00, 0x01};//固定3字节头,跟annexb没什么关系
    memcpy(pes, pes_start_prefix, 3);
    pes += 3;
    // stream_id
    *pes++ = TsPESStreamIdVideoCommon;
    // PES_packet_length
    uint8_t PTS_DTS_flags = 0x03;
    if (header.composition_time == 0) {//dts = pts时，只需要dts
        PTS_DTS_flags = 0x02;
    }

    //PES_header_data_length
    uint8_t PES_header_data_length = 0;
    if (PTS_DTS_flags == 0x02) {
        PES_header_data_length = 5;//DTS 5字节
        pes_header.dts = pes_header.pts = video_pkt->tag_header.timestamp*90;
    } else if (PTS_DTS_flags == 0x03) {
        PES_header_data_length = 10;//PTS 5字节
        pes_header.dts = video_pkt->tag_header.timestamp*90;
        pes_header.pts = (video_pkt->tag_header.timestamp + header.composition_time)*90;
    }

    uint32_t PES_packet_length_tmp = 3 + PES_header_data_length + payload_size;
    uint16_t PES_packet_length = PES_packet_length_tmp > 0xffff?0:PES_packet_length_tmp;
    *((uint16_t*)pes) = htons(PES_packet_length);
    pes_header.PES_packet_length = PES_packet_length;

    pes += 2;
    // 10' 2 bslbf
    // PES_scrambling_control 2 bslbf 
    // PES_priority 1 bslbf 
    // data_alignment_indicator 1 bslbf 
    // copyright 1 bslbf 
    // original_or_copy 1 bslbf 
    *pes++ = 0x80;
    // PTS_DTS_flags 2 bslbf 
    // ESCR_flag 1 bslbf 
    // ES_rate_flag 1 bslbf 
    // DSM_trick_mode_flag 1 bslbf 
    // additional_copy_info_flag 1 bslbf 
    // PES_CRC_flag 1 bslbf 
    // PES_extension_flag
    *pes++ = PTS_DTS_flags << 6;

    pes_header.PTS_DTS_flags = PTS_DTS_flags;
    pes_header.PES_header_data_length = PES_header_data_length;

    if (PTS_DTS_flags & 0x02) {
        pes_header.pts = (video_pkt->tag_header.timestamp + header.composition_time)*90;
    }

    if (PTS_DTS_flags & 0x01) {
        pes_header.dts = video_pkt->tag_header.timestamp*90;
    }

    *pes++ = PES_header_data_length;
    VIDEODATA *video_data = (VIDEODATA *)video_pkt->tag_data.get();
    if (PTS_DTS_flags & 0x02) {// 填充pts
        uint64_t pts = (video_pkt->tag_header.timestamp + video_data->header.composition_time)*90;
        int32_t val = 0;
        val = int32_t(0x02 << 4 | (((pts >> 30) & 0x07) << 1) | 1);
        *pes++ = val;

        val = int32_t((((pts >> 15) & 0x7fff) << 1) | 1);
        *pes++ = (val >> 8);
        *pes++ = val;

        val = int32_t((((pts)&0x7fff) << 1) | 1);
        *pes++ = (val >> 8);
        *pes++ = val;
    }

    if (PTS_DTS_flags & 0x01) {// 填充dts
        uint64_t dts = video_pkt->tag_header.timestamp*90;
        int32_t val = 0;
        val = int32_t(0x03 << 4 | (((dts >> 30) & 0x07) << 1) | 1);
        *pes++ = val;

        val = int32_t((((dts >> 15) & 0x7fff) << 1) | 1);
        *pes++ = (val >> 8);
        *pes++ = val;

        val = int32_t((((dts)&0x7fff) << 1) | 1);
        *pes++ = (val >> 8);
        *pes++ = val;
    }

    int32_t video_pes_header_bytes = pes - video_pes_header_;
    int32_t video_pes_len = video_pes_header_bytes;
    video_pes_segs_.clear();
    video_pes_segs_.emplace_back(std::string_view(video_pes_header_, video_pes_len));

    static char annexb_start_code1[] = { 0x00, 0x00, 0x01 };
    static char annexb_start_code2[] = { 0x00, 0x00, 0x00, 0x01 };    
    bool first_nalu = true;
    for (auto & nalu : nalus) {
        if (first_nalu) {
            first_nalu = false;
            video_pes_segs_.emplace_back(std::string_view(annexb_start_code2, 4));
            video_pes_len += 4;
        } else {
            memcpy(pes, annexb_start_code1, 3);
            video_pes_segs_.emplace_back(std::string_view(annexb_start_code1, 3));
            video_pes_len += 3;
        }
        video_pes_segs_.emplace_back(nalu);
        video_pes_len += nalu.size();
    }
    
    pes_packet->alloc_buf(video_pes_len);
    for (auto & seg : video_pes_segs_) {
        auto unuse_payload = pes_packet->get_unuse_data();
        memcpy((void*)unuse_payload.data(), seg.data(), seg.size());
        pes_packet->inc_used_bytes(seg.size());
    }
    pes_packet->is_key = is_key;
    // 生成pes
    // 切成ts切片
    create_video_ts(pes_packet, video_pes_len, is_key);
    curr_seg_->update_video_dts(video_pkt->tag_header.timestamp + video_data->header.composition_time);
    ts_media_source_->on_pes_packet(pes_packet);
    return true;
}


bool FlvToTs::process_h265_packet(std::shared_ptr<FlvTag> video_pkt) {
    VideoTagHeader header;
    auto payload = video_pkt->get_using_data();
    payload.remove_prefix(FLV_TAG_HEADER_BYTES);
    int32_t header_consumed = header.decode((uint8_t*)payload.data(), payload.size());
    if (header_consumed < 0) {
        return false;
    }

    if (!video_codec_) {
        return false;
    }
    
    HevcCodec *hevc_codec = ((HevcCodec*)video_codec_.get());
    if (header.is_seq_header()) {// 关键帧索引
        video_ready_ = true;
        video_header_ = video_pkt;
        // 解析hvcc configuration heade
        HEVCDecoderConfigurationRecord hevc_decoder_configuration_record;
        int32_t consumed = hevc_decoder_configuration_record.decode((uint8_t*)payload.data() + header_consumed, payload.size() - header_consumed);
        if (consumed == 0) {
            return false;
        }

        hevc_codec->set_hevc_configuration(hevc_decoder_configuration_record);
        nalu_length_size_ = hevc_decoder_configuration_record.lengthSizeMinusOne + 1;
        if (nalu_length_size_ == 3 || nalu_length_size_ > 4) {
            return false;
        }
        return true;
    } 

    bool is_key = header.is_key_frame() && !header.is_seq_header();
    if (!curr_seg_ && !is_key) {//片段开始的帧，必须是关键帧
        return false;
    }

    auto pes_packet = std::make_shared<PESPacket>();// pes_packet;
    auto & pes_header = pes_packet->pes_header;
    memset((void*)&pes_header, 0, sizeof(pes_header));

    if (curr_seg_) {
        if (publish_app_->can_reap_ts(is_key, curr_seg_)) {
            HLS_INFO("session:{}, reap ts seq:{}, name:{}, bytes:{}k, dur:{} by video", 
                            get_session_name(), 
                            curr_seg_->get_seqno(),
                            curr_seg_->get_filename(),
                            curr_seg_->get_ts_bytes()/1024,
                            curr_seg_->get_duration()
            );
            curr_seg_->set_reaped();
            on_ts_segment(curr_seg_);
            curr_seg_ = nullptr;
        }
    }

    if (!curr_seg_) {
        curr_seg_ = std::make_shared<TsSegment>();
        std::string_view pat_seg = curr_seg_->alloc_ts_packet();
        create_pat(pat_seg);
        std::string_view pmt_seg = curr_seg_->alloc_ts_packet();
        create_pmt(pmt_seg);
    }
    // 获取到nalus
    std::list<std::string_view> nalus;
    auto consumed = get_nalus((uint8_t*)payload.data() + header_consumed, payload.size() - header_consumed, nalus);
    if (consumed < 0) {
        return false;
    }

    // 判断vps,sps,pps,aud等
    // bool has_aud_nalu = false;
    bool has_sps_nalu = false;
    bool has_pps_nalu = false;
    bool has_vps_nalu = false;
    bool has_key_nalu = false;
    for (auto it = nalus.begin(); it != nalus.end(); it++) {
        HEVC_NALU_TYPE nalu_type = H265_TYPE(*it->data());
        if (nalu_type == NAL_SPS) {
            has_sps_nalu = true;
            hevc_codec->set_sps(std::string((it)->data(), (it)->size()));
        } else if (nalu_type == NAL_PPS) {
            has_pps_nalu = true;
            hevc_codec->set_pps(std::string((it)->data(), (it)->size()));
        } else if (nalu_type == NAL_VPS) {
            has_vps_nalu = true;
            hevc_codec->set_vps(std::string((it)->data(), (it)->size()));
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

    // 计算payload长度(第一个nalu头部4个字节，后面的头部只需要3字节)
    int32_t payload_size = nalus.size()*3 + 1;//头部的总字节数
    for (auto & nalu : nalus) {
        payload_size += nalu.size();
    }

    // 添加上pes header
    // uint8_t *pes = video_pes_.get();
    pes_header.stream_id = TsPESStreamIdVideoCommon;
    char *pes = video_pes_header_;
    static char pes_start_prefix[3] = {0x00, 0x00, 0x01};//固定3字节头,跟annexb没什么关系
    memcpy(pes, pes_start_prefix, 3);
    pes += 3;
    // stream_id
    *pes++ = TsPESStreamIdVideoCommon;
    // PES_packet_length
    VIDEODATA *video_data = (VIDEODATA *)video_pkt->tag_data.get();
    uint8_t PTS_DTS_flags = 0x03;
    if (video_data->header.composition_time == 0) {//dts = pts时，只需要dts
        PTS_DTS_flags = 0x02;
    }

    //PES_header_data_length
    uint8_t PES_header_data_length = 0;
    
    if (PTS_DTS_flags == 0x02) {
        PES_header_data_length = 5;//DTS 5字节
        pes_header.dts = pes_header.pts = video_pkt->tag_header.timestamp*90;
    } else if (PTS_DTS_flags == 0x03) {
        PES_header_data_length = 10;//PTS 5字节
        pes_header.dts = video_pkt->tag_header.timestamp*90;
        pes_header.pts = (video_pkt->tag_header.timestamp + video_data->header.composition_time)*90;
    }

    uint32_t PES_packet_length_tmp = 3 + PES_header_data_length + payload_size;
    uint16_t PES_packet_length = PES_packet_length_tmp > 0xffff?0:PES_packet_length_tmp;
    *((uint16_t*)pes) = htons(PES_packet_length);
    pes += 2;
    // 10' 2 bslbf
    // PES_scrambling_control 2 bslbf 
    // PES_priority 1 bslbf 
    // data_alignment_indicator 1 bslbf 
    // copyright 1 bslbf 
    // original_or_copy 1 bslbf 
    *pes++ = 0x80;
    // PTS_DTS_flags 2 bslbf 
    // ESCR_flag 1 bslbf 
    // ES_rate_flag 1 bslbf 
    // DSM_trick_mode_flag 1 bslbf 
    // additional_copy_info_flag 1 bslbf 
    // PES_CRC_flag 1 bslbf 
    // PES_extension_flag
    *pes++ = PTS_DTS_flags << 6;

    pes_header.PTS_DTS_flags = PTS_DTS_flags;
    pes_header.PES_header_data_length = PES_header_data_length;

    if (PTS_DTS_flags & 0x02) {
        pes_header.pts = (video_pkt->tag_header.timestamp + header.composition_time)*90;
    }

    if (PTS_DTS_flags & 0x01) {
        pes_header.dts = video_pkt->tag_header.timestamp*90;
    }

    *pes++ = PES_header_data_length;
    if (PTS_DTS_flags & 0x02) {// 填充pts
        uint64_t pts = (video_pkt->tag_header.timestamp + video_data->header.composition_time)*90;
        int32_t val = 0;
        val = int32_t(0x02 << 4 | (((pts >> 30) & 0x07) << 1) | 1);
        *pes++ = val;

        val = int32_t((((pts >> 15) & 0x7fff) << 1) | 1);
        *pes++ = (val >> 8);
        *pes++ = val;

        val = int32_t((((pts)&0x7fff) << 1) | 1);
        *pes++ = (val >> 8);
        *pes++ = val;
    }

    if (PTS_DTS_flags & 0x01) {// 填充dts
        uint64_t dts = video_pkt->tag_header.timestamp*90;
        int32_t val = 0;
        val = int32_t(0x03 << 4 | (((dts >> 30) & 0x07) << 1) | 1);
        *pes++ = val;

        val = int32_t((((dts >> 15) & 0x7fff) << 1) | 1);
        *pes++ = (val >> 8);
        *pes++ = val;

        val = int32_t((((dts)&0x7fff) << 1) | 1);
        *pes++ = (val >> 8);
        *pes++ = val;
    }

    int32_t video_pes_len = pes - video_pes_header_;
    video_pes_segs_.clear();
    video_pes_segs_.emplace_back(std::string_view(video_pes_header_, video_pes_len));

    static char annexb_start_code1[] = { 0x00, 0x00, 0x01 };
    static char annexb_start_code2[] = { 0x00, 0x00, 0x00, 0x01 };    
    bool first_nalu = true;
    for (auto & nalu : nalus) {
        if (first_nalu) {
            first_nalu = false;
            video_pes_segs_.emplace_back(std::string_view(annexb_start_code2, 4));
            video_pes_len += 4;
        } else {
            memcpy(pes, annexb_start_code1, 3);
            video_pes_segs_.emplace_back(std::string_view(annexb_start_code1, 3));
            video_pes_len += 3;
        }

        video_pes_segs_.emplace_back(nalu);
        video_pes_len += nalu.size();
    }

    pes_packet->alloc_buf(video_pes_len);
    for (auto & seg : video_pes_segs_) {
        auto unuse_payload = pes_packet->get_unuse_data();
        memcpy((void*)unuse_payload.data(), seg.data(), seg.size());
        pes_packet->inc_used_bytes(seg.size());
    }
    // 生成pes
    // 切成ts切片
    create_video_ts(pes_packet, video_pes_len, is_key);
    curr_seg_->update_video_dts(video_pkt->tag_header.timestamp + video_data->header.composition_time);
    ts_media_source_->on_pes_packet(pes_packet);
    return true;
}

void FlvToTs::create_pat(std::string_view & data) {
    uint8_t *buf = (uint8_t*)data.data();
    /*********************** ts header *********************/
    *buf++ = 0x47;//ts header sync byte
    // transport_error_indicator(1b)        0
    // payload_unit_start_indicator(1b)     1
    // transport_priority(1b)               0
    // pid(13b)                             TsPidPAT
    int16_t v = (0 << 15) | (1 << 14) | (0 << 13) | TsPidPAT;
    (*(uint16_t*)buf) = htons(v);
    buf += 2;
    // transport_scrambling_control(2b) = 00
    // adaptation_field_control(2b) = payload only
    // continuity_counter_(4b)   = 0
    *buf++ = (00 << 6) | (TsAdapationControlPayloadOnly << 4) | 00;
    /********************** psi header *********************/
    //payload_unit_start_indicator = 1, pointer_field = 0
    *buf++ = 0;
    uint8_t *pat_start = buf;
    // table_id
    *buf++ = TsPsiTableIdPas;
    // 2B
    // The section_syntax_indicator is a 1-bit field which shall be set to '1'.
    // int8_t section_syntax_indicator; //1bit
    // // const value, must be '0'
    // int8_t const0_value; //1bit
    // // reverved value, must be '1'
    // int8_t const1_value; //2bits
    // // This is a 12-bit field, the first two bits of which shall be '00'. The remaining 10 bits specify the number
    // // of bytes of the section, starting immediately following the section_length field, and including the CRC. The value in this
    // // field shall not exceed 1021 (0x3FD).
    // uint16_t section_length; //12bits
    // section_length = psi_size + 4;
    // psi_size = transport_stream_id + reserved + version_number + current_next_indicator + section_number + last_section_number + program_number
    
    // section_length
    int16_t section_len = 5 + 4 + 4;//transport_stream_id(2B) + current_next_indicator(1B) + section_number(1B) + last_section_number(1B) + crc
    int16_t slv = section_len & 0x0FFF;
    slv |= (1 << 15) & 0x8000;
    slv |= (0 << 14) & 0x4000;
    slv |= (3 << 12) & 0x3000;
    *((uint16_t*)buf) = htons(slv);
    buf += 2;
    //transport_stream_id
    *((uint16_t*)buf) = htons(0x0001);//transport_stream_id
    buf += 2;
    // 1B
    int8_t cniv = 0x01;//current_next_indicator 1b 1
    cniv |= (0 << 1) & 0x3E;//version_number 5b 00000
    cniv |= (3 << 6) & 0xC0;//reserved 2b 11
    *buf++ = cniv;
    // section_number
    *buf++ = 0;
    // last_section_number
    *buf++ = 0;
    // program
    int16_t pmt_number = TS_PMT_NUMBER;
    int16_t pmt_pid = TS_PMT_PID;
    uint32_t v32;
    v32 = (pmt_pid & 0x1FFF) | (0x07 << 13) | ((pmt_number << 16) & 0xFFFF0000);
    *((uint32_t *)buf) = htonl(v32);
    buf += 4;
    //crc32
    uint32_t crc32 = Utils::calc_mpeg_ts_crc32(pat_start, buf - pat_start);
    *(uint32_t *)buf = htonl(crc32);
    buf += 4;
    memset(buf, 0xFF, (uint8_t*)data.data() + 188 - buf);
    return;
}

void FlvToTs::create_pmt(std::string_view & pmt_seg) {
    uint8_t *buf = (uint8_t*)pmt_seg.data();
    /*********************** ts header *********************/
    *buf = 0x47;//ts header sync byte
    buf++;
    // transport_error_indicator(1b)        0
    // payload_unit_start_indicator(1b)     1
    // transport_priority(1b)               0
    // pid(13b)                             TS_PMT_PID
    int16_t v = (0 << 15) | (1 << 14) | (0 << 13) | TS_PMT_PID;
    (*(uint16_t*)buf) = htons(v);
    buf += 2;
    // transport_scrambling_control(2b) = 00
    // adaptation_field_control(2b) = payload only
    // continuity_counter_(4b)   = 0
    *buf = (TsAdapationControlPayloadOnly << 4);
    buf++;
    /********************** psi header *********************/
    //payload_unit_start_indicator = 1, pointer_field = 0
    *buf = 0;
    buf++;
    uint8_t *pmt_start = buf;
    // table_id
    *buf = TsPsiTableIdPms;
    buf++;
    // 2B
    // The section_syntax_indicator is a 1-bit field which shall be set to '1'.
    // int8_t section_syntax_indicator; //1bit
    // // const value, must be '0'
    // int8_t const0_value; //1bit
    // // reverved value, must be '1'
    // int8_t const1_value; //2bits
    // // This is a 12-bit field, the first two bits of which shall be '00'. The remaining 10 bits specify the number
    // // of bytes of the section, starting immediately following the section_length field, and including the CRC. The value in this
    // // field shall not exceed 1021 (0x3FD).
    // uint16_t section_length; //12bits
    // section_length = psi_size + 4;
    // psi_size = transport_stream_id + reserved + version_number + current_next_indicator + section_number + last_section_number + program_number
    
    // section_length
    int16_t section_len = 5 + 4 + 4;//5 + PCR_PID(2B) + program_info_length(2B) + VIDEO(5B) + AUDIO(5B) + crc32(4B)
    if (has_video_) {
        section_len += 5;
    }

    if (has_audio_) {
        section_len += 5;
    }
    int16_t slv = section_len & 0x0FFF;
    slv |= (1 << 15) & 0x8000;
    slv |= (0 << 14) & 0x4000;
    slv |= (3 << 12) & 0x3000;
    *((uint16_t*)buf) = htons(slv);
    buf += 2;

    // program number
    *((uint16_t*)buf) = htons(TS_PMT_NUMBER);//program number
    buf += 2;
    // 1B
    int8_t cniv = 0x01;//current_next_indicator 1b 1
    cniv |= (0 << 1) & 0x3E;//version_number 5b 00000
    cniv |= (3 << 6) & 0xC0;//reserved 2b 11
    *buf = cniv;
    buf++;
    // section_number
    *buf = 0;
    buf++;
    // last_section_number
    *buf = 0;
    buf++;
    // reserved 3 bslbf 
    // PCR_PID 13 uimsbf
    // 2B
    int16_t ppv = PCR_PID & 0x1FFF;
    ppv |= (7 << 13) & 0xE000;
    *((uint16_t*)buf) = htons(ppv);
    buf += 2;
    // reserved 4 bslbf 
    // program_info_length 12 0
    // 2B
    int16_t pilv = 0 & 0xFFF;
    pilv |= (0xf << 12) & 0xF000;
    *((uint16_t*)buf) = htons(pilv);
    buf += 2;
    // for (i = 0; i < N1; i++) { 
    //     stream_type 8 uimsbf 
    //     reserved 3 bslbf 
    //     elementary_PID 13 uimsbf 
        
    //     reserved 4 bslbf 
    //     ES_info_length 12 uimsbf 
    //     for (i = 0; i < N2; i++) { 
    //         descriptor() 
    //     } 
    // }

    if (has_audio_) {
        *buf = audio_type_;
        buf++;
        int16_t epv = audio_pid_ & 0x1FFF;
        epv |= (0x7 << 13) & 0xE000;
        *((uint16_t*)buf) = htons(epv);
        buf += 2;
        int16_t eilv = 0 & 0x0FFF;
        eilv |= (0xf << 12) & 0xF000;
        *((uint16_t*)buf) = htons(eilv);
        buf += 2;
    }

    if (has_video_) {
        *buf = video_type_;
        buf++;
        int16_t epv = video_pid_ & 0x1FFF;
        epv |= (0x7 << 13) & 0xE000;
        *((uint16_t*)buf) = htons(epv);
        buf += 2;
        int16_t eilv = 0 & 0x0FFF;
        eilv |= (0xf << 12) & 0xF000;
        *((uint16_t*)buf) = htons(eilv);
        buf += 2;
    }

    //crc32
    uint32_t crc32 = Utils::calc_mpeg_ts_crc32(pmt_start, buf - pmt_start);
    *(uint32_t *)buf = htonl(crc32);
    buf += 4;
    memset(buf, 0xFF, (uint8_t*)pmt_seg.data() + 188 - buf);
    return;
}

void FlvToTs::create_video_ts(std::shared_ptr<PESPacket> pes_packet, int32_t pes_len, bool is_key) {
    int32_t left_count = pes_len;
    int32_t curr_pes_seg_index = 0;
    pes_packet->ts_index = curr_seg_->get_curr_ts_chunk_index();
    pes_packet->ts_off = curr_seg_->get_curr_ts_chunk_offset();
    pes_packet->ts_seg = curr_seg_;
    int32_t ts_total_bytes = 0;
    while (left_count > 0) {
        int32_t space_left = 184;
        std::string_view ts_seg = curr_seg_->alloc_ts_packet();
        ts_total_bytes += 188;
        uint8_t *buf = (uint8_t*)ts_seg.data();
        *buf++ = 0x47;//ts header sync byte

        uint8_t payload_unit_start_indicator = 0;
        if (left_count == pes_len) {// 第一个ts切片，置上标志位
            payload_unit_start_indicator = 1;
        }
        // transport_error_indicator(1b)        0
        // payload_unit_start_indicator(1b)     1
        // transport_priority(1b)               0
        // pid(13b)                             TsPidPAT
        int16_t v = (0 << 15) | (payload_unit_start_indicator << 14) | (0 << 13) | video_pid_;
        (*(uint16_t*)buf) = htons(v);
        buf += 2;

        
        bool write_pcr = false;
        if (left_count == pes_len) {// 第一个切片
            if (PCR_PID == video_pid_ && is_key) {
                write_pcr = true;
            }
        }

        bool write_padding = false;
        if (left_count < space_left) {
            write_padding = true;
        }

        // transport_scrambling_control(2b) = 00
        // adaptation_field_control(2b) = payload only
        // continuity_counter_(4b)   = 0
        if (write_pcr || write_padding) {
            *buf++ = (00 << 6 ) | (TsAdapationControlBoth << 4) | (continuity_counter_[video_pid_] & 0x0f);
        } else {
            *buf++ = (00 << 6) | (TsAdapationControlPayloadOnly << 4) | (continuity_counter_[video_pid_] & 0x0f);
        }
        continuity_counter_[video_pid_]++;

        int adaptation_field_total_length = 0;
        if (write_pcr) {
            // adaptation_field_length
            adaptation_field_total_length = 8;// adaptation_field_length(1) + flags(1) + PCR(6) = 8
            int staffing_count = 0;
            if ((4 + adaptation_field_total_length + left_count) <= 188) {
                staffing_count = 188 - (4 + adaptation_field_total_length + left_count);
            } else {
                staffing_count = 0;
            }

            if (staffing_count > 0) {
                adaptation_field_total_length += staffing_count;
            }
            int adaptation_field_length = adaptation_field_total_length - 1;// 需要减掉自己的1字节
            *buf++ = adaptation_field_length;
            *buf++ = 0x10;// (PCR_flag << 4) & 0x10; 有pcr，直接写成0x10
            int64_t pcrv = (0) & 0x1ff;//program_clock_reference_base
            pcrv |= (0x3f << 9) & 0x7E00;//reserved
            pcrv |= (pes_packet->pes_header.dts << 15) & 0xFFFFFFFF8000LL;

            char *pp = (char*)&pcrv;
            *buf++ = pp[5];
            *buf++ = pp[4];
            *buf++ = pp[3];
            *buf++ = pp[2];
            *buf++ = pp[1];
            *buf++ = pp[0];
            // 如果存在填充，则填充尾部0xff
            memset(buf, 0xff, staffing_count);
            buf += staffing_count;
            space_left -= adaptation_field_total_length;
        }

        if (write_padding) {
            adaptation_field_total_length = 2;// adaptation_field_length + flags = 2
            int staffing_count = 0;
            if ((4 + adaptation_field_total_length + left_count) <= 188) {
                staffing_count = 188 - (4 + adaptation_field_total_length + left_count);
            } else {
                staffing_count = 0;
            }

            if (staffing_count > 0) {
                adaptation_field_total_length += staffing_count;
            }
            int adaptation_field_length = adaptation_field_total_length - 1;
            *buf++ = adaptation_field_length;
            *buf++ = 0x00;//(PCR_flag << 4) & 0x10;   末尾的，不需要pcr
            memset(buf, 0xff, staffing_count);
            buf += staffing_count;
        }

        int32_t consumed = 184 - adaptation_field_total_length;
        int32_t left_consume = consumed;
        int32_t buff_off = 0;
        while (left_consume > 0) {
            int32_t curr_seg_size = video_pes_segs_[curr_pes_seg_index].size();
            if (curr_seg_size < left_consume) {
                memcpy(buf + buff_off, video_pes_segs_[curr_pes_seg_index].data(), curr_seg_size);
                curr_pes_seg_index++;
                left_consume -= curr_seg_size;
                buff_off += curr_seg_size;
            } else if (curr_seg_size == left_consume) {// 可以覆盖
                memcpy(buf + buff_off, video_pes_segs_[curr_pes_seg_index].data(), left_consume);
                curr_pes_seg_index++;
                buff_off += left_consume;
                left_consume = 0;
            } else {//curr_seg_size > left_consume
                memcpy(buf + buff_off, video_pes_segs_[curr_pes_seg_index].data(), left_consume);
                video_pes_segs_[curr_pes_seg_index].remove_prefix(left_consume);
                left_consume = 0;
                buff_off += left_consume;
            }
        }
        buf += buff_off;
        // memcpy(buf, p, consumed);
        // p += consumed;
        left_count -= consumed;
    }
    pes_packet->ts_bytes = ts_total_bytes;
}

int32_t FlvToTs::get_nalus(uint8_t *data, int32_t len, std::list<std::string_view> & nalus) {
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

bool FlvToTs::on_audio_packet(std::shared_ptr<FlvTag> audio_pkt) {
    if (!audio_codec_) {
        return false;
    }

    if (audio_codec_->get_codec_type() == CODEC_AAC) {
        return process_aac_packet(audio_pkt);
    } else if (audio_codec_->get_codec_type() == CODEC_MP3) {
        return process_mp3_packet(audio_pkt);
    }

    return false;
}

bool FlvToTs::process_aac_packet(std::shared_ptr<FlvTag> audio_pkt) {
    AudioTagHeader header;
    auto payload = audio_pkt->get_using_data();
    payload.remove_prefix(FLV_TAG_HEADER_BYTES);
    int32_t header_consumed = header.decode((uint8_t*)payload.data(), payload.size());
    if (header_consumed < 0) {
        return false;
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
            CORE_ERROR("parse aac audio header failed, ret:{}", consumed);
            return false;
        }

        aac_codec->set_audio_specific_config(audio_config);
        return true;
    }

    if (sequence_header) {
        return true;
    }

    if (!audio_ready_) {
        return false;
    }

    auto pes_packet = std::make_shared<PESPacket>();// pes_packet;
    auto & pes_header = pes_packet->pes_header;
    memset((void*)&pes_header, 0, sizeof(pes_header));

    if (curr_seg_) {
        if (publish_app_->can_reap_ts(false, curr_seg_)) {
            HLS_INFO("session:{}, reap ts seq:{}, name:{}, bytes:{}k, dur:{} by audio", 
                            get_session_name(), 
                            curr_seg_->get_seqno(),
                            curr_seg_->get_filename(),
                            curr_seg_->get_ts_bytes()/1024,
                            curr_seg_->get_duration()
            );
            on_ts_segment(curr_seg_);
            curr_seg_ = nullptr;
        }
    }

    auto audio_config = aac_codec->get_audio_specific_config();
    // profile, 2bits
    uint8_t aac_profile = 0;
    if (audio_config->audio_object_type == AOT_AAC_MAIN) {
        aac_profile = AdtsAacProfileMain;
    } else if (audio_config->audio_object_type == AOT_AAC_LC) {
        aac_profile = AdtsAacProfileLC;
    } else if (audio_config->audio_object_type == AOT_AAC_SSR) {
        aac_profile = AdtsAacProfileSSR;
    } else {
        return false;
    }

    audio_buf_.audio_pkts.emplace_back(audio_pkt);
    if (audio_buf_.audio_pes_len == 0) {
        audio_buf_.timestamp = audio_pkt->tag_header.timestamp;
    }

    int32_t audio_payload_size = payload.size() - header_consumed;
    int32_t frame_length = 7 + audio_payload_size;
    auto & adts_header = adts_headers_[adts_header_index_];
    adts_header.data[0] = 0xff;
    adts_header.data[1] = 0xf9;
    adts_header.data[2] = (aac_profile << 6) & 0xc0;
    // sampling_frequency_index 4bits
    adts_header.data[2] |= (audio_config->frequency_index << 2) & 0x3c;
    // channel_configuration 3bits
    adts_header.data[2] |= (audio_config->channel_configuration >> 2) & 0x01;
    adts_header.data[3] = (audio_config->channel_configuration << 6) & 0xc0;
    // frame_length 13bits
    // payload_size = payload.size() - header_consumed + 7;
    adts_header.data[3] |= (frame_length >> 11) & 0x03;
    adts_header.data[4] = (frame_length >> 3) & 0xff;
    adts_header.data[5] = ((frame_length << 5) & 0xe0);
    // adts_buffer_fullness; //11bits
    adts_header.data[5] |= 0x1f;
    adts_header.data[6] = 0xfc;
    // uint8_t adts_header[7] = { 0xff, 0xf9, 0x00, 0x00, 0x00, 0x0f, 0xfc };
    adts_header_index_++;
    
    audio_buf_.audio_pes_segs.emplace_back(std::string_view(adts_header.data, 7));
    audio_buf_.audio_pes_segs.emplace_back(std::string_view(payload.data() + header_consumed, audio_payload_size));
    audio_buf_.audio_pes_len += (7 + audio_payload_size);
    if (audio_buf_.audio_pes_len < 1400) {//至少1400字节，再一起送切片
        return true;
    }

    if (!curr_seg_) {
        curr_seg_ = std::make_shared<TsSegment>();
        std::string_view pat_seg = curr_seg_->alloc_ts_packet();
        create_pat(pat_seg);
        std::string_view pmt_seg = curr_seg_->alloc_ts_packet();
        create_pmt(pmt_seg);
    }

    // uint8_t *pes = audio_pes_.get();
    char *pes = audio_pes_header_;
    static char pes_start_prefix[3] = {0x00, 0x00, 0x01};
    memcpy(pes, pes_start_prefix, 3);
    pes += 3;
    // stream_id
    pes_header.stream_id = TsPESStreamIdAudioCommon;
    *pes++ = TsPESStreamIdAudioCommon;
    uint8_t PTS_DTS_flags = 0x02;
    uint8_t PES_header_data_length = 5;//只有pts
    // int32_t payload_size = 7 + payload.size() - header_consumed;//adts header + payload
    int32_t payload_size = audio_buf_.audio_pes_len;
    uint32_t PES_packet_length_tmp = 3 + PES_header_data_length + payload_size;
    uint16_t PES_packet_length = PES_packet_length_tmp > 0xffff?0:PES_packet_length_tmp;
    *((uint16_t*)pes) = htons(PES_packet_length);
    pes_header.PES_packet_length = PES_packet_length;
    pes += 2;
    // 10' 2 bslbf
    // PES_scrambling_control 2 bslbf 
    // PES_priority 1 bslbf 
    // data_alignment_indicator 1 bslbf 
    // copyright 1 bslbf 
    // original_or_copy 1 bslbf 
    *pes++ = 0x80;
    // PTS_DTS_flags 2 bslbf 
    // ESCR_flag 1 bslbf 
    // ES_rate_flag 1 bslbf 
    // DSM_trick_mode_flag 1 bslbf 
    // additional_copy_info_flag 1 bslbf 
    // PES_CRC_flag 1 bslbf 
    // PES_extension_flag
    *pes++ = PTS_DTS_flags << 6;
    *pes++ = PES_header_data_length;
    //audio pts
    uint64_t dts = audio_buf_.timestamp*90;
    pes_header.dts = dts;
    int32_t val = 0;
    val = int32_t(0x03 << 4 | (((dts >> 30) & 0x07) << 1) | 1);
    *pes++ = val;

    val = int32_t((((dts >> 15) & 0x7fff) << 1) | 1);
    *pes++ = (val >> 8);
    *pes++ = val;

    val = int32_t((((dts)&0x7fff) << 1) | 1);
    *pes++ = (val >> 8);
    *pes++ = val;
    
    // // audio pts end
    // uint8_t adts_header[7] = { 0xff, 0xf9, 0x00, 0x00, 0x00, 0x0f, 0xfc };
    // AudioSpecificConfig & audio_config = aac_codec->getAudioSpecificConfig();

    // // profile, 2bits
    // uint8_t aac_profile = 0;
    // if (audio_config.audio_object_type == AOT_AAC_MAIN) {
    //     aac_profile = AdtsAacProfileMain;
    // } else if (audio_config.audio_object_type == AOT_AAC_LC) {
    //     aac_profile = AdtsAacProfileLC;
    // } else if (audio_config.audio_object_type == AOT_AAC_SSR) {
    //     aac_profile = AdtsAacProfileSSR;
    // } else {
    //     return false;
    // }

    // adts_header[2] = (aac_profile << 6) & 0xc0;
    // // sampling_frequency_index 4bits
    // adts_header[2] |= (audio_config.frequency_index << 2) & 0x3c;
    // // channel_configuration 3bits
    // adts_header[2] |= (audio_config.channel_configuration >> 2) & 0x01;
    // adts_header[3] = (audio_config.channel_configuration << 6) & 0xc0;
    // // frame_length 13bits
    // // payload_size = payload.size() - header_consumed + 7;
    // adts_header[3] |= (payload_size >> 11) & 0x03;
    // adts_header[4] = (payload_size >> 3) & 0xff;
    // adts_header[5] = ((payload_size << 5) & 0xe0);
    // // adts_buffer_fullness; //11bits
    // adts_header[5] |= 0x1f;

    // memcpy(pes, adts_header, 7);
    // pes += 7;

    int32_t pes_header_len = pes - audio_pes_header_;
    audio_buf_.audio_pes_len += pes_header_len;
    audio_buf_.audio_pes_segs[0] = std::string_view(audio_pes_header_, pes_header_len);

    int32_t audio_pes_bytes = 0;
    for (auto seg : audio_buf_.audio_pes_segs) {
        audio_pes_bytes += seg.size();
    }
    
    pes_packet->alloc_buf(audio_pes_bytes);
    for (auto seg : audio_buf_.audio_pes_segs) {
        auto unuse_payload = pes_packet->get_unuse_data();
        memcpy((void*)unuse_payload.data(), seg.data(), seg.size());
        pes_packet->inc_used_bytes(seg.size());
    }

    create_audio_ts(pes_packet);
    curr_seg_->update_audio_pts(audio_buf_.timestamp);
    ts_media_source_->on_pes_packet(pes_packet);
    audio_buf_.clear();
    adts_header_index_ = 0;
    return true;
}

bool FlvToTs::process_mp3_packet(std::shared_ptr<FlvTag> audio_pkt) {
    AudioTagHeader header;
    auto payload = audio_pkt->get_using_data();
    payload.remove_prefix(FLV_TAG_HEADER_BYTES);
    int32_t header_consumed = header.decode((uint8_t*)payload.data(), payload.size());
    if (header_consumed < 0) {
        return false;
    }

    auto pes_packet = std::make_shared<PESPacket>();// pes_packet;
    auto & pes_header = pes_packet->pes_header;
    memset((void*)&pes_header, 0, sizeof(pes_header));

    if (curr_seg_) {
        if (publish_app_->can_reap_ts(false, curr_seg_)) {
            HLS_INFO("session:{}, reap ts seq:{}, name:{}, bytes:{}k, dur:{} by audio", 
                            get_session_name(), 
                            curr_seg_->get_seqno(),
                            curr_seg_->get_filename(),
                            curr_seg_->get_ts_bytes()/1024,
                            curr_seg_->get_duration()
            );
            on_ts_segment(curr_seg_);
            curr_seg_ = nullptr;
        }
    }

    int32_t audio_payload_size = payload.size() - header_consumed;
    audio_buf_.audio_pes_segs.emplace_back(std::string_view(payload.data() + header_consumed, audio_payload_size));
    audio_buf_.audio_pes_len += audio_payload_size;

    if (!curr_seg_) {
        curr_seg_ = std::make_shared<TsSegment>();
        std::string_view pat_seg = curr_seg_->alloc_ts_packet();
        create_pat(pat_seg);
        std::string_view pmt_seg = curr_seg_->alloc_ts_packet();
        create_pmt(pmt_seg);
    }

    char *pes = audio_pes_header_;
    static char pes_start_prefix[3] = {0x00, 0x00, 0x01};
    memcpy(pes, pes_start_prefix, 3);
    pes += 3;
    // stream_id
    pes_header.stream_id = TsPESStreamIdAudioCommon;
    *pes++ = TsPESStreamIdAudioCommon;
    uint8_t PTS_DTS_flags = 0x02;
    uint8_t PES_header_data_length = 5;//只有pts

    int32_t payload_size = audio_buf_.audio_pes_len;
    uint32_t PES_packet_length_tmp = 3 + PES_header_data_length + payload_size;
    uint16_t PES_packet_length = PES_packet_length_tmp > 0xffff?0:PES_packet_length_tmp;
    *((uint16_t*)pes) = htons(PES_packet_length);
    pes_header.PES_packet_length = PES_packet_length;
    pes += 2;
    // 10' 2 bslbf
    // PES_scrambling_control 2 bslbf 
    // PES_priority 1 bslbf 
    // data_alignment_indicator 1 bslbf 
    // copyright 1 bslbf 
    // original_or_copy 1 bslbf 
    *pes++ = 0x80;
    // PTS_DTS_flags 2 bslbf 
    // ESCR_flag 1 bslbf 
    // ES_rate_flag 1 bslbf 
    // DSM_trick_mode_flag 1 bslbf 
    // additional_copy_info_flag 1 bslbf 
    // PES_CRC_flag 1 bslbf 
    // PES_extension_flag
    *pes++ = PTS_DTS_flags << 6;
    *pes++ = PES_header_data_length;
    //audio pts
    uint64_t dts = audio_pkt->tag_header.timestamp*90;
    pes_header.dts = dts;
    int32_t val = 0;
    val = int32_t(0x03 << 4 | (((dts >> 30) & 0x07) << 1) | 1);
    *pes++ = val;

    val = int32_t((((dts >> 15) & 0x7fff) << 1) | 1);
    *pes++ = (val >> 8);
    *pes++ = val;

    val = int32_t((((dts)&0x7fff) << 1) | 1);
    *pes++ = (val >> 8);
    *pes++ = val;
    
    int32_t pes_header_len = pes - audio_pes_header_;
    audio_buf_.audio_pes_len += pes_header_len;
    audio_buf_.audio_pes_segs[0] = std::string_view(audio_pes_header_, pes_header_len);

    int32_t audio_pes_bytes = 0;
    for (auto seg : audio_buf_.audio_pes_segs) {
        audio_pes_bytes += seg.size();
    }
    
    pes_packet->alloc_buf(audio_pes_bytes);
    for (auto seg : audio_buf_.audio_pes_segs) {
        auto unuse_payload = pes_packet->get_unuse_data();
        memcpy((void*)unuse_payload.data(), seg.data(), seg.size());
        pes_packet->inc_used_bytes(seg.size());
    }

    create_audio_ts(pes_packet);
    curr_seg_->update_audio_pts(audio_pkt->tag_header.timestamp);
    ts_media_source_->on_pes_packet(pes_packet);
    audio_buf_.clear();
    return true;
}

void FlvToTs::create_audio_ts(std::shared_ptr<PESPacket> pes_packet) {
    int curr_pes_seg_index = 0;
    int32_t left_count = audio_buf_.audio_pes_len;
    pes_packet->ts_index = curr_seg_->get_curr_ts_chunk_index();
    pes_packet->ts_off = curr_seg_->get_curr_ts_chunk_offset();
    pes_packet->ts_seg = curr_seg_;
    int32_t ts_total_bytes = 0;

    while (left_count > 0) {
        int32_t space_left = 184;
        std::string_view ts_seg = curr_seg_->alloc_ts_packet();
        ts_total_bytes += 188;
        uint8_t *buf = (uint8_t*)ts_seg.data();
        *buf++ = 0x47;//ts header sync byte
        // transport_error_indicator(1b)        0
        // payload_unit_start_indicator(1b)     1
        // transport_priority(1b)               0
        // pid(13b)                             TsPidPAT
        uint8_t payload_unit_start_indicator = 0;
        if (left_count == audio_buf_.audio_pes_len) {
            payload_unit_start_indicator = 1;
        }
        int16_t v = (0 << 15) | (payload_unit_start_indicator << 14) | (0 << 13) | audio_pid_;
        (*(uint16_t*)buf) = htons(v);
        buf += 2;
        // transport_scrambling_control(2b) = 00
        // adaptation_field_control(2b) = payload only
        // continuity_counter_(4b)   = 0
        bool write_padding = false;
        if (left_count < space_left) {
            write_padding = true;
        }

        if (write_padding) {
            *buf++ = (00 << 6 ) | (TsAdapationControlBoth << 4) | (continuity_counter_[audio_pid_] & 0x0f);
        } else {
            *buf++ = (00 << 6) | (TsAdapationControlPayloadOnly << 4) | (continuity_counter_[audio_pid_] & 0x0f);
        }

        continuity_counter_[audio_pid_]++;
        int adaptation_field_total_length = 0;
        if (write_padding) {
            adaptation_field_total_length = 2;// adaptation_field_length + flags = 2
            int staffing_count = 0;
            if ((4 + adaptation_field_total_length + left_count) <= 188) {
                staffing_count = 188 - (4 + adaptation_field_total_length + left_count);
            } else {
                staffing_count = 0;
            }

            if (staffing_count > 0) {
                adaptation_field_total_length += staffing_count;
            }
            int adaptation_field_length = adaptation_field_total_length - 1;
            *buf++ = adaptation_field_length;
            *buf++ = 0x00;//(PCR_flag << 4) & 0x10;   末尾的，不需要pcr
            memset(buf, 0xff, staffing_count);
            buf += staffing_count;
        }

        int consumed = 184 - adaptation_field_total_length;

        int32_t left_consume = consumed;
        int32_t buff_off = 0;
        while (left_consume > 0) {
            int32_t curr_seg_size = audio_buf_.audio_pes_segs[curr_pes_seg_index].size();
            if (curr_seg_size < left_consume) {
                memcpy(buf + buff_off, audio_buf_.audio_pes_segs[curr_pes_seg_index].data(), curr_seg_size);
                curr_pes_seg_index++;
                left_consume -= curr_seg_size;
                buff_off += curr_seg_size;
            } else if (curr_seg_size == left_consume) {// 可以覆盖
                memcpy(buf + buff_off, audio_buf_.audio_pes_segs[curr_pes_seg_index].data(), left_consume);
                curr_pes_seg_index++;
                buff_off += left_consume;
                left_consume = 0;
            } else {//curr_seg_size > left_consume
                memcpy(buf + buff_off, audio_buf_.audio_pes_segs[curr_pes_seg_index].data(), left_consume);
                audio_buf_.audio_pes_segs[curr_pes_seg_index].remove_prefix(left_consume);
                left_consume = 0;
            }
        }
        buf += buff_off;
        left_count -= consumed;
    }
    pes_packet->ts_bytes = ts_total_bytes;
}


void FlvToTs::on_ts_segment(std::shared_ptr<TsSegment> seg) {
    ts_media_source_->on_ts_segment(seg);
}

void FlvToTs::close() {
    if (closed_.test_and_set(std::memory_order_acquire)) {
        return;
    }

    auto self(shared_from_this());
    boost::asio::co_spawn(worker_->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        check_closable_timer_.cancel();
        co_await wg_.wait();

        if (ts_media_source_) {
            ts_media_source_->close();
            ts_media_source_ = nullptr;
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