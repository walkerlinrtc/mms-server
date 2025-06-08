/*
 * @Author: jbl19860422
 * @Date: 2023-11-09 20:49:39
 * @LastEditTime: 2023-12-27 22:14:52
 * @LastEditors: jbl19860422
 * @Description:
 * @FilePath: \mms\mms\server\transcode\webrtc_to_ts.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved.
 */
#include "webrtc_to_ts.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>

#include "app/publish_app.h"
#include "base/utils/utils.h"
#include "codec/aac/aac_codec.hpp"
#include "codec/aac/aac_encoder.hpp"
#include "codec/aac/adts.hpp"
#include "codec/aac/mpeg4_aac.hpp"
#include "codec/av1/av1_codec.hpp"
#include "codec/h264/h264_codec.hpp"
#include "codec/hevc/hevc_codec.hpp"
#include "codec/opus/opus_codec.hpp"
#include "core/rtp_media_sink.hpp"
#include "protocol/rtmp/flv/flv_define.hpp"
#include "protocol/rtmp/flv/flv_tag.hpp"
#include "protocol/rtmp/rtmp_message/data_message/rtmp_metadata_message.hpp"
#include "protocol/ts/ts_header.hpp"
#include "protocol/ts/ts_pat_pmt.hpp"
#include "protocol/ts/ts_segment.hpp"
#include "spdlog/spdlog.h"

using namespace mms;
WebRtcToTs::WebRtcToTs(ThreadWorker *worker, std::shared_ptr<PublishApp> app,
                       std::weak_ptr<MediaSource> origin_source, const std::string &domain_name,
                       const std::string &app_name, const std::string &stream_name)
    : MediaBridge(worker, app, origin_source, domain_name, app_name, stream_name),
      check_closable_timer_(worker->get_io_context()),
      wg_(worker) {
    sink_ = std::make_shared<RtpMediaSink>(worker);
    rtp_media_sink_ = std::static_pointer_cast<RtpMediaSink>(sink_);
    source_ = std::make_shared<TsMediaSource>(
        worker, std::weak_ptr<StreamSession>(std::shared_ptr<StreamSession>(nullptr)), publish_app_);
    ts_media_source_ = std::static_pointer_cast<TsMediaSource>(source_);
    video_pes_segs_.reserve(1024);
    aac_buf_ = std::make_unique<char[]>(8192 * 10);
    video_frame_cache_ = std::make_unique<char[]>(1024 * 1024);
    audio_frame_cache_ = std::make_unique<char[]>(1024 * 20);
    spdlog::debug("create webrtc to ts");
    if (swr_context_) {
        swr_free(&swr_context_);
        swr_context_ = nullptr;
    }
    type_ = "webrtc-to-ts";
}

WebRtcToTs::~WebRtcToTs() {}

bool WebRtcToTs::init() {
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

                if (ts_media_source_->has_no_sinks_for_time(30000)) {  // 已经30秒没人播放了
                    spdlog::debug("close WebRtcToTs because no players for 30s");
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
                    exit(0);
                }
            }
            if (swr_init(swr_context_) < 0) {
                spdlog::error("swr init failed");
                exit(0);
            }

            aac_encoder_ = std::make_unique<AACEncoder>();
            aac_encoder_->init(44100, opus_codec->get_channels());
            has_audio_ = true;
        }

        if (has_video_) {
            if (video_codec_->get_codec_type() == CODEC_H264) {
                video_codec_ = std::make_shared<H264Codec>();
                video_pid_ = TS_VIDEO_AVC_PID;
                video_type_ = TsStreamVideoH264;
                continuity_counter_[video_pid_] = 0;
            }
        }

        if (has_audio_) {
            audio_pid_ = TS_AUDIO_AAC_PID;
            audio_type_ = TsStreamAudioAAC;
            continuity_counter_[audio_pid_] = 0;
            adts_headers_.reserve(200);
        }

        if (has_video_) {
            PCR_PID = video_pid_;
        } else if (has_audio_) {
            PCR_PID = audio_pid_;
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
                process_audio_packet(pkt, (pkt->get_timestamp() - first_rtp_audio_ts_) / 48);
            }

            co_return true;
        });
    return true;
}

void WebRtcToTs::process_video_packet(std::shared_ptr<RtpPacket> pkt) {
    if (video_codec_ && video_codec_->get_codec_type() == CODEC_H264) {
        process_h264_packet(pkt);
    }
}

void WebRtcToTs::process_h264_packet(std::shared_ptr<RtpPacket> pkt) {
    auto h264_nalu = rtp_h264_depacketizer_.on_packet(pkt);
    if (h264_nalu) {
        int64_t this_timestamp = h264_nalu->get_timestamp();
        generate_h264_ts((this_timestamp - first_rtp_video_ts_) / 90,
                         h264_nalu);  // todo 除90这个要根据sdp来计算，目前固定
    }
}

void WebRtcToTs::generate_h264_ts(int64_t timestamp, std::shared_ptr<RtpH264NALU> &nalu) {
    bool is_key = false;
    char *buf = video_frame_cache_.get();
    auto &pkts = nalu->get_rtp_pkts();

    std::string_view sps;
    std::string_view pps;
    std::list<std::string_view> nalus;

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
                    sps = std::string_view(buf + 4, nalu_size);
                } else if (nalu_type == H264NaluTypePPS) {
                    pps = std::string_view(buf + 4, nalu_size);
                }

                uint32_t s = htonl(nalu_size);
                memcpy(buf, &s, 4);
                buf += 4;
                memcpy(buf, data + 2, nalu_size);
                nalus.emplace_back(std::string_view((char *)data + 2, nalu_size));
                buf += nalu_size;

                pos += 2 + nalu_size;
                data += 2 + nalu_size;
            }

            if (pkt->get_header().marker == 1) {  // 最后一个
                break;
            }
        } else if (pkt_info.is_single_nalu()) {
            if (pkt_info.get_nalu_type() == H264NaluTypeIDR) {
                is_key = true;
            } else if (pkt_info.get_nalu_type() == H264NaluTypeSPS) {
                sps = std::string_view(buf + 4, pkt->get_payload().size());
            } else if (pkt_info.get_nalu_type() == H264NaluTypePPS) {
                pps = std::string_view(buf + 4, pkt->get_payload().size());
            }

            uint32_t s = htonl(pkt->get_payload().size());
            memcpy(buf, &s, 4);
            buf += 4;
            memcpy(buf, pkt->get_payload().data(), pkt->get_payload().size());
            nalus.emplace_back(
                std::string_view((char *)pkt->get_payload().data(), pkt->get_payload().size()));
            buf += pkt->get_payload().size();

            if (pkt->get_header().marker == 1) {
                break;
            }
        } else if (pkt_info.get_type() == H264_RTP_PAYLOAD_FU_A) {
            int32_t nalu_size = 0;
            int32_t *nalu_size_buf_pos = (int32_t *)buf;
            buf += 4;
            char *buf_data_start = buf;
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
                        if (pkt_info.get_nalu_type() == H264NaluTypeSPS) {
                            sps = std::string_view(buf_data_start, nalu_size);
                        } else if (pkt_info.get_nalu_type() == H264NaluTypePPS) {
                            pps = std::string_view(buf_data_start, nalu_size);
                        }

                        nalus.push_back(std::string_view(buf_data_start, nalu_size));
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
                break;
            } else {
                spdlog::error("H264_RTP_PAYLOAD_FU_A nalu not marker");
            }
        }
    }

    if (sps.size() > 0) {
        if (video_codec_ && video_codec_->get_codec_type() == CODEC_H264) {
            H264Codec *h264_codec = (H264Codec *)video_codec_.get();
            h264_codec->set_sps(std::string(sps.data(), sps.size()));
        }
    }

    if (pps.size() > 0) {
        if (video_codec_ && video_codec_->get_codec_type() == CODEC_H264) {
            H264Codec *h264_codec = (H264Codec *)video_codec_.get();
            h264_codec->set_pps(std::string(pps.data(), pps.size()));
        }
    }

    if (wait_first_key_frame_) {
        if (!is_key) {
            return;
        }
        wait_first_key_frame_ = false;
    }

    if (!curr_seg_ && !is_key) {  // 片段开始的帧，必须是关键帧
        return;
    }

    if (curr_seg_) {
        if (publish_app_->can_reap_ts(is_key, curr_seg_)) {
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

    auto pes_packet = std::make_shared<PESPacket>();  // pes_packet;
    auto &pes_header = pes_packet->pes_header;
    memset((void *)&pes_header, 0, sizeof(pes_header));

    // 判断sps,pps,aud等
    bool has_aud_nalu = false;
    bool has_sps_nalu = false;
    bool has_pps_nalu = false;

    H264Codec *h264_codec = (H264Codec *)video_codec_.get();
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
                it = nalus.insert(it, std::string_view(h264_codec->get_pps_nalu().data(),
                                                       h264_codec->get_pps_nalu().size()));
                it = nalus.insert(it, std::string_view(h264_codec->get_sps_nalu().data(),
                                                       h264_codec->get_sps_nalu().size()));
                has_pps_nalu = true;
                has_sps_nalu = true;
            }
        }
    }

    static uint8_t default_aud_nalu[] = {0x09, 0xf0};
    static std::string_view aud_nalu((char *)default_aud_nalu, 2);
    if (!has_aud_nalu) {
        nalus.push_front(aud_nalu);
    }
    // 计算payload长度(第一个nalu头部4个字节，后面的头部只需要3字节)
    int32_t payload_size = nalus.size() * 3 + 1;  // 头部的总字节数
    for (auto &nalu : nalus) {
        payload_size += nalu.size();
    }

    // 添加上pes header
    // uint8_t *pes = video_pes_.get();
    char *pes = video_pes_header_;
    static char pes_start_prefix[3] = {0x00, 0x00, 0x01};  // 固定3字节头,跟annexb没什么关系
    memcpy(pes, pes_start_prefix, 3);
    pes += 3;
    // stream_id
    *pes++ = TsPESStreamIdVideoCommon;
    // PES_packet_length
    uint8_t PTS_DTS_flags = 0x03;
    // if (header.composition_time == 0) {//dts = pts时，只需要dts
    PTS_DTS_flags = 0x02;
    // }

    // PES_header_data_length
    uint8_t PES_header_data_length = 0;
    if (PTS_DTS_flags == 0x02) {
        PES_header_data_length = 5;  // DTS 5字节
    } else if (PTS_DTS_flags == 0x03) {
        PES_header_data_length = 10;  // PTS 5字节
    }

    uint32_t PES_packet_length_tmp = 3 + PES_header_data_length + payload_size;
    uint16_t PES_packet_length = PES_packet_length_tmp > 0xffff ? 0 : PES_packet_length_tmp;
    *((uint16_t *)pes) = htons(PES_packet_length);
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
    if (PTS_DTS_flags & 0x02) {  // 填充pts
        // uint64_t pts = (video_pkt->timestamp_ + header.composition_time)*90;
        uint64_t pts = timestamp * 90;
        int32_t val = 0;
        val = int32_t(0x02 << 4 | (((pts >> 30) & 0x07) << 1) | 1);
        *pes++ = val;

        val = int32_t((((pts >> 15) & 0x7fff) << 1) | 1);
        *pes++ = (val >> 8);
        *pes++ = val;

        val = int32_t((((pts) & 0x7fff) << 1) | 1);
        *pes++ = (val >> 8);
        *pes++ = val;
    }

    if (PTS_DTS_flags & 0x01) {  // 填充dts
        uint64_t dts = timestamp * 90;
        int32_t val = 0;
        val = int32_t(0x03 << 4 | (((dts >> 30) & 0x07) << 1) | 1);
        *pes++ = val;

        val = int32_t((((dts >> 15) & 0x7fff) << 1) | 1);
        *pes++ = (val >> 8);
        *pes++ = val;

        val = int32_t((((dts) & 0x7fff) << 1) | 1);
        *pes++ = (val >> 8);
        *pes++ = val;
    }

    int32_t video_pes_len = pes - video_pes_header_;
    video_pes_segs_.clear();
    video_pes_segs_.emplace_back(std::string_view(video_pes_header_, video_pes_len));

    static char annexb_start_code1[] = {0x00, 0x00, 0x01};
    static char annexb_start_code2[] = {0x00, 0x00, 0x00, 0x01};
    bool first_nalu = true;
    for (auto &nalu : nalus) {
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
    for (auto &seg : video_pes_segs_) {
        auto unuse_payload = pes_packet->get_unuse_data();
        memcpy((void *)unuse_payload.data(), seg.data(), seg.size());
        pes_packet->inc_used_bytes(seg.size());
    }

    // 生成pes
    // 切成ts切片
    pes_packet->is_key = is_key;
    create_video_ts(pes_packet, video_pes_len, is_key);
    curr_seg_->update_video_dts(timestamp);
    ts_media_source_->on_pes_packet(pes_packet);
}

void WebRtcToTs::create_pat(std::string_view &data) {
    uint8_t *buf = (uint8_t *)data.data();
    /*********************** ts header *********************/
    *buf++ = 0x47;  // ts header sync byte
    // transport_error_indicator(1b)        0
    // payload_unit_start_indicator(1b)     1
    // transport_priority(1b)               0
    // pid(13b)                             TsPidPAT
    int16_t v = (0 << 15) | (1 << 14) | (0 << 13) | TsPidPAT;
    (*(uint16_t *)buf) = htons(v);
    buf += 2;
    // transport_scrambling_control(2b) = 00
    // adaptation_field_control(2b) = payload only
    // continuity_counter_(4b)   = 0
    *buf++ = (00 << 6) | (TsAdapationControlPayloadOnly << 4) | 00;
    /********************** psi header *********************/
    // payload_unit_start_indicator = 1, pointer_field = 0
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
    // // This is a 12-bit field, the first two bits of which shall be '00'. The remaining 10 bits specify the
    // number
    // // of bytes of the section, starting immediately following the section_length field, and including the
    // CRC. The value in this
    // // field shall not exceed 1021 (0x3FD).
    // uint16_t section_length; //12bits
    // section_length = psi_size + 4;
    // psi_size = transport_stream_id + reserved + version_number + current_next_indicator + section_number +
    // last_section_number + program_number

    // section_length
    int16_t section_len = 5 + 4 + 4;  // transport_stream_id(2B) + current_next_indicator(1B) +
                                      // section_number(1B) + last_section_number(1B) + crc
    int16_t slv = section_len & 0x0FFF;
    slv |= (1 << 15) & 0x8000;
    slv |= (0 << 14) & 0x4000;
    slv |= (3 << 12) & 0x3000;
    *((uint16_t *)buf) = htons(slv);
    buf += 2;
    // transport_stream_id
    *((uint16_t *)buf) = htons(0x0001);  // transport_stream_id
    buf += 2;
    // 1B
    int8_t cniv = 0x01;       // current_next_indicator 1b 1
    cniv |= (0 << 1) & 0x3E;  // version_number 5b 00000
    cniv |= (3 << 6) & 0xC0;  // reserved 2b 11
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
    // crc32
    uint32_t crc32 = Utils::calc_mpeg_ts_crc32(pat_start, buf - pat_start);
    *(uint32_t *)buf = htonl(crc32);
    buf += 4;
    memset(buf, 0xFF, (uint8_t *)data.data() + 188 - buf);
    return;
}

void WebRtcToTs::create_pmt(std::string_view &pmt_seg) {
    uint8_t *buf = (uint8_t *)pmt_seg.data();
    /*********************** ts header *********************/
    *buf = 0x47;  // ts header sync byte
    buf++;
    // transport_error_indicator(1b)        0
    // payload_unit_start_indicator(1b)     1
    // transport_priority(1b)               0
    // pid(13b)                             TS_PMT_PID
    int16_t v = (0 << 15) | (1 << 14) | (0 << 13) | TS_PMT_PID;
    (*(uint16_t *)buf) = htons(v);
    buf += 2;
    // transport_scrambling_control(2b) = 00
    // adaptation_field_control(2b) = payload only
    // continuity_counter_(4b)   = 0
    *buf = (TsAdapationControlPayloadOnly << 4);
    buf++;
    /********************** psi header *********************/
    // payload_unit_start_indicator = 1, pointer_field = 0
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
    // // This is a 12-bit field, the first two bits of which shall be '00'. The remaining 10 bits specify the
    // number
    // // of bytes of the section, starting immediately following the section_length field, and including the
    // CRC. The value in this
    // // field shall not exceed 1021 (0x3FD).
    // uint16_t section_length; //12bits
    // section_length = psi_size + 4;
    // psi_size = transport_stream_id + reserved + version_number + current_next_indicator + section_number +
    // last_section_number + program_number

    // section_length
    int16_t section_len =
        5 + 4 + 4;  // 5 + PCR_PID(2B) + program_info_length(2B) + VIDEO(5B) + AUDIO(5B) + crc32(4B)
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
    *((uint16_t *)buf) = htons(slv);
    buf += 2;

    // program number
    *((uint16_t *)buf) = htons(TS_PMT_NUMBER);  // program number
    buf += 2;
    // 1B
    int8_t cniv = 0x01;       // current_next_indicator 1b 1
    cniv |= (0 << 1) & 0x3E;  // version_number 5b 00000
    cniv |= (3 << 6) & 0xC0;  // reserved 2b 11
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
    *((uint16_t *)buf) = htons(ppv);
    buf += 2;
    // reserved 4 bslbf
    // program_info_length 12 0
    // 2B
    int16_t pilv = 0 & 0xFFF;
    pilv |= (0xf << 12) & 0xF000;
    *((uint16_t *)buf) = htons(pilv);
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
        *((uint16_t *)buf) = htons(epv);
        buf += 2;
        int16_t eilv = 0 & 0x0FFF;
        eilv |= (0xf << 12) & 0xF000;
        *((uint16_t *)buf) = htons(eilv);
        buf += 2;
    }

    if (has_video_) {
        *buf = video_type_;
        buf++;
        int16_t epv = video_pid_ & 0x1FFF;
        epv |= (0x7 << 13) & 0xE000;
        *((uint16_t *)buf) = htons(epv);
        buf += 2;
        int16_t eilv = 0 & 0x0FFF;
        eilv |= (0xf << 12) & 0xF000;
        *((uint16_t *)buf) = htons(eilv);
        buf += 2;
    }

    // crc32
    uint32_t crc32 = Utils::calc_mpeg_ts_crc32(pmt_start, buf - pmt_start);
    *(uint32_t *)buf = htonl(crc32);
    buf += 4;
    memset(buf, 0xFF, (uint8_t *)pmt_seg.data() + 188 - buf);
    return;
}

void WebRtcToTs::create_video_ts(std::shared_ptr<PESPacket> pes_packet, int32_t pes_len, bool is_key) {
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
        uint8_t *buf = (uint8_t *)ts_seg.data();
        *buf++ = 0x47;  // ts header sync byte

        uint8_t payload_unit_start_indicator = 0;
        if (left_count == pes_len) {  // 第一个ts切片，置上标志位
            payload_unit_start_indicator = 1;
        }
        // transport_error_indicator(1b)        0
        // payload_unit_start_indicator(1b)     1
        // transport_priority(1b)               0
        // pid(13b)                             TsPidPAT
        int16_t v = (0 << 15) | (payload_unit_start_indicator << 14) | (0 << 13) | video_pid_;
        (*(uint16_t *)buf) = htons(v);
        buf += 2;

        bool write_pcr = false;
        if (left_count == pes_len) {  // 第一个切片
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
            *buf++ = (00 << 6) | (TsAdapationControlBoth << 4) | (continuity_counter_[video_pid_] & 0x0f);
        } else {
            *buf++ =
                (00 << 6) | (TsAdapationControlPayloadOnly << 4) | (continuity_counter_[video_pid_] & 0x0f);
        }
        continuity_counter_[video_pid_]++;

        int adaptation_field_total_length = 0;
        if (write_pcr) {
            // adaptation_field_length
            adaptation_field_total_length = 8;  // adaptation_field_length(1) + flags(1) + PCR(6) = 8
            int staffing_count = 0;
            if ((4 + adaptation_field_total_length + left_count) <= 188) {
                staffing_count = 188 - (4 + adaptation_field_total_length + left_count);
            } else {
                staffing_count = 0;
            }

            if (staffing_count > 0) {
                adaptation_field_total_length += staffing_count;
            }
            int adaptation_field_length = adaptation_field_total_length - 1;  // 需要减掉自己的1字节
            *buf++ = adaptation_field_length;
            *buf++ = 0x10;                 // (PCR_flag << 4) & 0x10; 有pcr，直接写成0x10
            int64_t pcrv = (0) & 0x1ff;    // program_clock_reference_base
            pcrv |= (0x3f << 9) & 0x7E00;  // reserved
            pcrv |= (pes_packet->pes_header.dts << 15) & 0xFFFFFFFF8000LL;

            char *pp = (char *)&pcrv;
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
            adaptation_field_total_length = 2;  // adaptation_field_length + flags = 2
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
            *buf++ = 0x00;  //(PCR_flag << 4) & 0x10;   末尾的，不需要pcr
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
            } else if (curr_seg_size == left_consume) {  // 可以覆盖
                memcpy(buf + buff_off, video_pes_segs_[curr_pes_seg_index].data(), left_consume);
                curr_pes_seg_index++;
                buff_off += left_consume;
                left_consume = 0;
            } else {  // curr_seg_size > left_consume
                memcpy(buf + buff_off, video_pes_segs_[curr_pes_seg_index].data(), left_consume);
                video_pes_segs_[curr_pes_seg_index].remove_prefix(left_consume);
                left_consume = 0;
                buff_off += left_consume;
            }
        }
        buf += buff_off;
        left_count -= consumed;
    }
    pes_packet->ts_bytes = ts_total_bytes;
}

void WebRtcToTs::process_audio_packet(std::shared_ptr<RtpPacket> pkt, int64_t timestamp) {
    if (audio_codec_ && audio_codec_->get_codec_type() == CODEC_OPUS) {
        process_opus_packet(pkt, timestamp);
    }
}

void WebRtcToTs::process_opus_packet(std::shared_ptr<RtpPacket> pkt, int64_t timestamp) {
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

    auto decoder = opus_decoder_.get();
    int32_t aac_bytes = 0;
    int in_samples = opus_decode(decoder, (uint8_t *)pkt->get_payload().data(), pkt->get_payload().size(),
                                 decoded_pcm_, 4096, 0);
    if (swr_context_) {
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

        aac_bytes = aac_encoder_->encode((uint8_t *)resampled_pcm_, total_samples * 2 * 2, aac_data_, 8192);
    }

    if (aac_bytes <= 0) {
        return;
    }

    memcpy(aac_buf_.get() + aac_buf_bytes_, aac_data_, aac_bytes);
    char *aac_payload = aac_buf_.get() + aac_buf_bytes_;
    aac_buf_bytes_ += aac_bytes;

    if (curr_seg_) {
        if (publish_app_->can_reap_ts(false, curr_seg_)) {
            on_ts_segment(curr_seg_);
            curr_seg_ = nullptr;
        }
    }

    audio_buf_.audio_pkts.emplace_back(pkt);
    if (audio_buf_.audio_pes_len == 0) {
        audio_buf_.timestamp = timestamp;
    }

    int32_t audio_payload_size = aac_bytes;
    int32_t frame_length = 7 + audio_payload_size;
    auto &adts_header = adts_headers_[adts_header_index_];
    uint8_t aac_profile = AdtsAacProfileLC;
    adts_header.data[0] = 0xff;
    adts_header.data[1] = 0xf9;
    adts_header.data[2] = (aac_profile << 6) & 0xc0;
    // sampling_frequency_index 4bits
    adts_header.data[2] |= (4 << 2) & 0x3c;  // 44100
    // channel_configuration 3bits(2 channel)
    adts_header.data[2] |= (2 >> 2) & 0x01;
    adts_header.data[3] = (2 << 6) & 0xc0;
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
    audio_buf_.audio_pes_segs.emplace_back(std::string_view(aac_payload, audio_payload_size));
    audio_buf_.audio_pes_len += (7 + audio_payload_size);
    if (audio_buf_.audio_pes_len < 1400) {  // 至少1400字节，再一起送切片
        return;
    }

    if (!curr_seg_) {
        curr_seg_ = std::make_shared<TsSegment>();
        std::string_view pat_seg = curr_seg_->alloc_ts_packet();
        create_pat(pat_seg);
        std::string_view pmt_seg = curr_seg_->alloc_ts_packet();
        create_pmt(pmt_seg);
    }

    auto pes_packet = std::make_shared<PESPacket>();  // pes_packet;
    auto &pes_header = pes_packet->pes_header;
    memset((void *)&pes_header, 0, sizeof(pes_header));

    char *pes = audio_pes_header_;
    static char pes_start_prefix[3] = {0x00, 0x00, 0x01};
    memcpy(pes, pes_start_prefix, 3);
    pes += 3;
    // stream_id
    *pes++ = TsPESStreamIdAudioCommon;
    uint8_t PTS_DTS_flags = 0x02;
    uint8_t PES_header_data_length = 5;  // 只有pts
    // int32_t payload_size = 7 + payload.size() - header_consumed;//adts header + payload
    int32_t payload_size = audio_buf_.audio_pes_len;
    uint32_t PES_packet_length_tmp = 3 + PES_header_data_length + payload_size;
    uint16_t PES_packet_length = PES_packet_length_tmp > 0xffff ? 0 : PES_packet_length_tmp;
    *((uint16_t *)pes) = htons(PES_packet_length);
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
    // audio pts
    uint64_t pts = audio_buf_.timestamp * 90;
    int32_t val = 0;
    val = int32_t(0x03 << 4 | (((pts >> 30) & 0x07) << 1) | 1);
    *pes++ = val;

    val = int32_t((((pts >> 15) & 0x7fff) << 1) | 1);
    *pes++ = (val >> 8);
    *pes++ = val;

    val = int32_t((((pts) & 0x7fff) << 1) | 1);
    *pes++ = (val >> 8);
    *pes++ = val;

    int32_t pes_header_len = pes - audio_pes_header_;
    audio_buf_.audio_pes_len += pes_header_len;
    audio_buf_.audio_pes_segs[0] = std::string_view(audio_pes_header_, pes_header_len);

    pes_packet->alloc_buf(audio_buf_.audio_pes_len);
    for (auto seg : audio_buf_.audio_pes_segs) {
        auto unuse_payload = pes_packet->get_unuse_data();
        memcpy((void *)unuse_payload.data(), seg.data(), seg.size());
        pes_packet->inc_used_bytes(seg.size());
    }

    create_audio_ts(pes_packet);
    curr_seg_->update_audio_pts(audio_buf_.timestamp);
    audio_buf_.clear();
    aac_buf_bytes_ = 0;
    adts_header_index_ = 0;
}

void WebRtcToTs::create_audio_ts(std::shared_ptr<PESPacket> pes_packet) {
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
        uint8_t *buf = (uint8_t *)ts_seg.data();
        *buf++ = 0x47;  // ts header sync byte
        // transport_error_indicator(1b)        0
        // payload_unit_start_indicator(1b)     1
        // transport_priority(1b)               0
        // pid(13b)                             TsPidPAT
        uint8_t payload_unit_start_indicator = 0;
        if (left_count == audio_buf_.audio_pes_len) {
            payload_unit_start_indicator = 1;
        }
        int16_t v = (0 << 15) | (payload_unit_start_indicator << 14) | (0 << 13) | audio_pid_;
        (*(uint16_t *)buf) = htons(v);
        buf += 2;
        // transport_scrambling_control(2b) = 00
        // adaptation_field_control(2b) = payload only
        // continuity_counter_(4b)   = 0
        bool write_padding = false;
        if (left_count < space_left) {
            write_padding = true;
        }

        if (write_padding) {
            *buf++ = (00 << 6) | (TsAdapationControlBoth << 4) | (continuity_counter_[audio_pid_] & 0x0f);
        } else {
            *buf++ =
                (00 << 6) | (TsAdapationControlPayloadOnly << 4) | (continuity_counter_[audio_pid_] & 0x0f);
        }

        continuity_counter_[audio_pid_]++;
        int adaptation_field_total_length = 0;
        if (write_padding) {
            adaptation_field_total_length = 2;  // adaptation_field_length + flags = 2
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
            *buf++ = 0x00;  //(PCR_flag << 4) & 0x10;   末尾的，不需要pcr
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
            } else if (curr_seg_size == left_consume) {  // 可以覆盖
                memcpy(buf + buff_off, audio_buf_.audio_pes_segs[curr_pes_seg_index].data(), left_consume);
                curr_pes_seg_index++;
                buff_off += left_consume;
                left_consume = 0;
            } else {  // curr_seg_size > left_consume
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

void WebRtcToTs::on_ts_segment(std::shared_ptr<TsSegment> seg) { ts_media_source_->on_ts_segment(seg); }

void WebRtcToTs::close() {
    if (closed_.test_and_set(std::memory_order_acquire)) {
        return;
    }

    auto self(shared_from_this());
    boost::asio::co_spawn(
        worker_->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            check_closable_timer_.cancel();
            co_await wg_.wait();

            if (ts_media_source_) {
                ts_media_source_->close();
                ts_media_source_ = nullptr;
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
        },
        boost::asio::detached);
}