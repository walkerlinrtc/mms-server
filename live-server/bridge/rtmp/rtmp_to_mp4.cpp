/*
 * @Author: jbl19860422
 * @Date: 2023-11-09 20:49:39
 * @LastEditTime: 2023-11-09 20:51:27
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\server\transcode\rtmp_to_ts.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>

#include <fstream>
#include "log/log.h"
#include "rtmp_to_mp4.hpp"
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
#include "config/app_config.h"


#include "mp4/moov.h"
#include "mp4/mvhd.h"
#include "mp4/ftyp.h"
#include "mp4/trak.h"
#include "mp4/tkhd.h"
#include "mp4/mdia.h"
#include "mp4/mdhd.h"
#include "mp4/hdlr.h"
#include "mp4/minf.h"
#include "mp4/vmhd.h"
#include "mp4/dinf.h"
#include "mp4/dref.h"
#include "mp4/stbl.h"
#include "mp4/stsd.h"
#include "mp4/visual_sample_entry.h"
#include "mp4/avcc.h"
#include "mp4/stss.h"
#include "mp4/stts.h"
#include "mp4/stsc.h"
#include "mp4/stsz.h"
#include "mp4/stco.h"
#include "mp4/mvex.h"
#include "mp4/trex.h"
#include "mp4/url.h"
#include "mp4/audio_sample_entry.h"
#include "mp4/esds.h"
#include "mp4/styp.h"
#include "mp4/sidx.h"
#include "mp4/mdia.h"
#include "mp4/mdat.h"
#include "mp4/moof.h"
#include "mp4/mfhd.h"
#include "mp4/tfhd.h"
#include "mp4/tfdt.h"
#include "mp4/trun.h"
#include "mp4/traf.h"


#include "core/mp4_media_source.hpp"

using namespace mms;
RtmpToMp4::RtmpToMp4(ThreadWorker *worker, std::shared_ptr<PublishApp> app, std::weak_ptr<MediaSource> origin_source, const std::string & domain_name, const std::string & app_name, const std::string & stream_name) : MediaBridge(worker, app, origin_source, domain_name, app_name, stream_name), check_closable_timer_(worker->get_io_context()), wg_(worker) {
    sink_ = std::make_shared<RtmpMediaSink>(worker);
    rtmp_media_sink_ = std::static_pointer_cast<RtmpMediaSink>(sink_);
    source_ = std::make_shared<Mp4MediaSource>(worker, std::weak_ptr<StreamSession>(std::shared_ptr<StreamSession>(nullptr)), publish_app_);
    mp4_media_source_ = std::static_pointer_cast<Mp4MediaSource>(source_);
    CORE_DEBUG("create rtmp to mp4");
    type_ = "rtmp-to-mp4";
}

RtmpToMp4::~RtmpToMp4() {

}

bool RtmpToMp4::init() {
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

            if (mp4_media_source_->has_no_sinks_for_time(app_conf->bridge_config().no_players_timeout_ms())) {//已经30秒没人播放了
                CORE_DEBUG("close RtmpToMp4 because no players for {}ms", app_conf->bridge_config().no_players_timeout_ms());
                close();
                break;
            }
        }
        co_return;
    }, [this, self](std::exception_ptr exp) {
        (void)exp;
        wg_.done();
    });

    rtmp_media_sink_->on_close([this, self]() {
        close();
    });

    rtmp_media_sink_->on_rtmp_message([this, self](const std::vector<std::shared_ptr<RtmpMessage>> & rtmp_msgs)->boost::asio::awaitable<bool> {
        for (auto rtmp_msg : rtmp_msgs) {
            if (rtmp_msg->get_message_type() == RTMP_MESSAGE_TYPE_AUDIO) {
                if (!this->on_audio_packet(rtmp_msg)) {
                    co_return false;
                }
            } else if (rtmp_msg->get_message_type() == RTMP_MESSAGE_TYPE_VIDEO) {
                if (!this->on_video_packet(rtmp_msg)) {
                    co_return false;
                }
            } else {
                if (!this->on_metadata(rtmp_msg)) {
                    co_return false;
                }
            }
        }
        
        co_return true;
    });
    return true;
}

bool RtmpToMp4::on_metadata(std::shared_ptr<RtmpMessage> metadata_pkt) {
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
        } else if (video_codec_id == VideoTagHeader::HEVC || video_codec_id == VideoTagHeader::HEVC_FOURCC) {
            video_codec_ = std::make_shared<HevcCodec>();
        } else {
            return false;
        }
    }

    if (has_audio_) {
        auto audio_codec_id = metadata_->get_audio_codec_id();
        if (audio_codec_id == AudioTagHeader::AAC) {
            audio_codec_ = std::make_shared<AACCodec>();
        } else if (audio_codec_id == AudioTagHeader::MP3) {
            audio_codec_ = std::make_shared<MP3Codec>();
        } else {
            return false;
        }
    }

    return true;
}

bool RtmpToMp4::on_video_packet(std::shared_ptr<RtmpMessage> video_pkt) {
    if (!video_codec_) {
        VideoTagHeader header;
        auto payload = video_pkt->get_using_data();
        header.decode((uint8_t*)payload.data(), payload.size());
        if (header.get_codec_id() == VideoTagHeader::AVC) {
            video_codec_ = std::make_shared<H264Codec>();
        } else if (header.get_codec_id() == VideoTagHeader::HEVC || header.get_codec_id() == VideoTagHeader::HEVC_FOURCC) {
            video_codec_ = std::make_shared<HevcCodec>();
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

bool RtmpToMp4::process_h264_packet(std::shared_ptr<RtmpMessage> video_pkt) {
    VideoTagHeader header;
    auto payload = video_pkt->get_using_data();
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
        generate_video_init_seg(video_pkt);
        return true;
    } 

    bool is_key = header.is_key_frame() && !header.is_seq_header();
    if (video_pkts_.size() <= 0 && !is_key) {//片段开始的帧，必须是关键帧
        return false;
    }

    if (video_pkts_.size() >= 2) {
        int64_t duration = video_pkts_[video_pkts_.size() - 1]->timestamp_ - video_pkts_[0]->timestamp_;
        if (publish_app_->can_reap_mp4(is_key, duration, video_bytes_)) {
            reap_video_seg(video_pkt->timestamp_);
            video_reaped_ = true;
            video_pkts_.clear();
            video_bytes_ = 0;
            mp4_media_source_->on_video_mp4_segment(video_data_mp4_seg_);
        }
    }

    video_bytes_ += payload.size() - header_consumed;
    video_pkts_.push_back(video_pkt);

    return true;
}

void RtmpToMp4::reap_video_seg(int64_t dts) {
    video_data_mp4_seg_ = std::make_shared<Mp4Segment>();
    std::stringstream ss;
    ss << "video-" << video_seq_no_ << ".m4s";
    std::ofstream of(ss.str(), std::ios::out|std::ios::binary);
    StypBox styp;
    styp.major_brand_ = Mp4BoxBrandMSDH;
    styp.minor_version_ = 0;
    styp.compatible_brands_.push_back(Mp4BoxBrandMSDH);
    styp.compatible_brands_.push_back(Mp4BoxBrandMSIX);
    size_t s = styp.size();
    NetBuffer n(video_data_mp4_seg_->alloc_buffer(s));
    styp.encode(n);


    auto sidx = std::make_shared<SidxBox>();
    sidx->version_ = 1;
    sidx->reference_id_ = 1;
    sidx->timescale_ = 1000;
    sidx->earliest_presentation_time_ = video_pkts_[0]->timestamp_;
    auto duration = video_pkts_[video_pkts_.size() - 1]->timestamp_ - sidx->earliest_presentation_time_;

    SegmentIndexEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.subsegment_duration = duration;
    entry.starts_with_SAP = 1;
    sidx->entries.push_back(entry);

    auto moof = std::make_shared<MoofBox>();

    auto mfhd = std::make_shared<MfhdBox>();
    mfhd->sequence_number_ = video_seq_no_;
    moof->mfhd_ = mfhd;

    auto traf = std::make_shared<TrafBox>();
    moof->traf_ = traf;

    auto tfhd = std::make_shared<TfhdBox>(0);
    tfhd->track_id_ = video_track_ID_;
    tfhd->flags_ = TfhdFlagsDefaultBaseIsMoof;
    traf->tfhd_ = tfhd;

    auto tfdt = std::make_shared<TfdtBox>();
    tfdt->base_media_decode_time = 1;//in ms
    tfdt->version_ = 1;
    traf->tfdt_ = tfdt;

    auto trun = std::make_shared<TrunBox>(0);
    trun->flags_ = TrunFlagsDataOffset | 
                   TrunFlagsSampleDuration | 
                   TrunFlagsSampleSize | 
                   TrunFlagsSampleFlag | 
                   TrunFlagsSampleCtsOffset;
    traf->trun_ = trun;
    trun->entries_.reserve(video_pkts_.size());
    std::shared_ptr<RtmpMessage> prev_pkt;
    int64_t mdat_bytes = 0;
    for (auto it = video_pkts_.begin(); it != video_pkts_.end(); it++) {
        TrunEntry te(trun.get());
        if (!prev_pkt) {
            prev_pkt = *it;
            te.sample_flags_ = 0x02000000;
        } else {
            te.sample_flags_ = 0x01000000;
        }

        auto it_next = it + 1;
        if (it_next == video_pkts_.end()) {
            te.sample_duration_ = dts - (*it)->timestamp_;
        } else {
            te.sample_duration_ = (*it_next)->timestamp_ - (*it)->timestamp_;
        }
        
        auto payload = (*it)->get_using_data();
        VideoTagHeader header;
        int32_t header_consumed = header.decode((uint8_t*)payload.data(), payload.size());
        te.sample_size_ = payload.size() - header_consumed;
        te.sample_composition_time_offset_ = header.composition_time;
        if (te.sample_composition_time_offset_ < 0) {
            trun->version_ = 1;
        }
        mdat_bytes += te.sample_size_;
        trun->entries_.push_back(te);
    }

    auto mdat = std::make_shared<MdatBox>();
    int64_t moof_bytes = moof->size();
    trun->data_offset_ = (int32_t)(moof_bytes + mdat->size());
    // Update the size of sidx.
    SegmentIndexEntry* e = &sidx->entries[0];
    e->referenced_size = moof_bytes +  mdat->size() + mdat_bytes;
    s = sidx->size();
    n = NetBuffer(video_data_mp4_seg_->alloc_buffer(s));
    sidx->encode(n);

    s = moof->size();
    n = NetBuffer(video_data_mp4_seg_->alloc_buffer(s));
    moof->encode(n);

    {//写mdat
        s = mdat->size();
        n = NetBuffer(video_data_mp4_seg_->alloc_buffer(s));
        mdat->encode(n);
        for (auto it = video_pkts_.begin(); it != video_pkts_.end(); it++) {
            auto payload = (*it)->get_using_data();
            VideoTagHeader header;
            int32_t header_consumed = header.decode((uint8_t*)payload.data(), payload.size());
            auto frame_bytes = payload.size() - header_consumed;
            n = NetBuffer(video_data_mp4_seg_->alloc_buffer(frame_bytes));
            memcpy((char*)n.get_curr_buf().data(), payload.data() + header_consumed, frame_bytes);
        }
    }
    video_data_mp4_seg_->update_timestamp(video_pkts_[0]->timestamp_, video_pkts_[video_pkts_.size()-1]->timestamp_);
    video_data_mp4_seg_->set_seqno(video_seq_no_);
    video_data_mp4_seg_->set_filename("video-" + std::to_string(video_seq_no_++) + ".m4s");
    auto used_buf = video_data_mp4_seg_->get_used_buf();
    of.write(used_buf.data(), used_buf.size());
    of.close();
}

bool RtmpToMp4::generate_video_init_seg(std::shared_ptr<RtmpMessage> video_pkt) {
    (void)video_pkt;
    std::ofstream of("./video-init.mp4", std::ios::out|std::ios::binary);
    H264Codec *h264_codec = ((H264Codec*)video_codec_.get());

    init_video_mp4_seg_ = std::make_shared<Mp4Segment>();
    FtypBox ftyp;
    ftyp.major_brand_ = Mp4BoxBrandISO5;
    ftyp.minor_version_ = 512;
    ftyp.compatible_brands_.push_back(Mp4BoxBrandISO6);
    ftyp.compatible_brands_.push_back(Mp4BoxBrandMP41);
    size_t s = ftyp.size();
    NetBuffer n(init_video_mp4_seg_->alloc_buffer(s));
    ftyp.encode(n);
    
    video_moov_ = std::make_shared<MoovBox>();
    std::shared_ptr<MvhdBox> mvhd = std::make_shared<MvhdBox>();
    mvhd->creation_time_ = time(NULL);
    mvhd->timescale_ = 1000;
    mvhd->duration_ = 0;
    mvhd->next_track_ID_ = video_track_ID_ + 1;
    video_moov_->add_box(mvhd);
    {
        //trak
        auto trak = std::make_shared<TrakBox>();
        video_moov_->add_box(trak);
        // tkhd
        auto tkhd = std::make_shared<TkhdBox>(0, 0x03);
        tkhd->track_ID_ = video_track_ID_;
        tkhd->duration_ = 0;
        uint32_t w,h;
        h264_codec->get_wh(w, h);
        tkhd->width_ = (w << 16);
        tkhd->height_ = (h << 16);
        trak->set_tkhd(tkhd);
        // mdia
        auto mdia = std::make_shared<MdiaBox>();
        trak->set_mdia(mdia);
        {
            // mdhd
            auto mdhd = std::make_shared<MdhdBox>();
            mdhd->timescale_ = 1000;
            mdhd->duration_ = 0;
            mdhd->set_language0('u');
            mdhd->set_language1('n');
            mdhd->set_language2('d');
            mdia->set_mdhd(mdhd);
            // hdlr
            auto hdlr = std::make_shared<HdlrBox>();
            hdlr->handler_type_ = HANDLER_TYPE_VIDE;
            hdlr->name_ = "VideoHandler";
            mdia->set_hdlr(hdlr);
            // minf
            auto minf = std::make_shared<MinfBox>();
            {
                auto vmhd = std::make_shared<VmhdBox>();
                minf->add_box(vmhd);
                auto dinf = std::make_shared<DinfBox>();
                minf->add_box(dinf);
                {
                    auto dref = std::make_shared<DrefBox>();
                    auto url = std::make_shared<UrlBox>();
                    // url->location_ = "same file";
                    dref->entries_.push_back(url);
                    dinf->set_dref(dref);
                }

                auto stbl = std::make_shared<StblBox>();
                {
                    
                    auto stts = std::make_shared<SttsBox>();
                    stbl->add_box(stts);
                    auto stsc = std::make_shared<StscBox>();
                    stbl->add_box(stsc);
                    auto stsz = std::make_shared<StszBox>();
                    stbl->add_box(stsz);
                    auto stco = std::make_shared<StcoBox>();
                    stbl->add_box(stco);

                    auto stsd = std::make_shared<StsdBox>();
                    {
                        auto avc1 = std::make_shared<VisualSampleEntry>(BOX_TYPE_AVC1);
                        stsd->entries_.push_back(avc1);
                        avc1->width_ = w;
                        avc1->height_ = h;
                        avc1->data_reference_index_ = 1;

                        auto avcc = std::make_shared<AvccBox>();
                        auto & avc_config = h264_codec->get_avc_configuration();
                        auto avc_size = avc_config.size();
                        std::string avc_raw_data;
                        avc_raw_data.resize(avc_size);
                        avc_config.encode((uint8_t*)avc_raw_data.data(), avc_raw_data.size());
                        avcc->avc_config_ = avc_raw_data;
                        avc1->set_avcc_box(avcc);
                    }
                    stbl->add_box(stsd);
                    
                }
                minf->add_box(stbl);
            }
            mdia->set_minf(minf);
        }   

    }

    auto mvex = std::make_shared<MvexBox>();
    auto trex = std::make_shared<TrexBox>();
    trex->track_ID_ = video_track_ID_;
    trex->default_sample_description_index_ = 1;
    mvex->set_trex(trex);
    video_moov_->add_box(mvex);
    
    video_moov_->size();
    n = NetBuffer(init_video_mp4_seg_->alloc_buffer(video_moov_->size()));
    video_moov_->encode(n);
    auto used_buf = init_video_mp4_seg_->get_used_buf();
    of.write(used_buf.data(), used_buf.size());
    of.close();
    init_video_mp4_seg_->set_filename("video-init.m4s");
    mp4_media_source_->on_video_init_segment(init_video_mp4_seg_);
    return true;
}

bool RtmpToMp4::generate_audio_init_seg(std::shared_ptr<RtmpMessage> audio_pkt) {
    std::ofstream of("./audio-init.mp4", std::ios::out|std::ios::binary);

    init_audio_mp4_seg_ = std::make_shared<Mp4Segment>();
    FtypBox ftyp;
    ftyp.major_brand_ = Mp4BoxBrandISO5;
    ftyp.minor_version_ = 512;
    ftyp.compatible_brands_.push_back(Mp4BoxBrandISO6);
    ftyp.compatible_brands_.push_back(Mp4BoxBrandMP41);
    size_t s = ftyp.size();
    NetBuffer n(init_audio_mp4_seg_->alloc_buffer(s));
    ftyp.encode(n);
    
    video_moov_ = std::make_shared<MoovBox>();
    std::shared_ptr<MvhdBox> mvhd = std::make_shared<MvhdBox>();
    mvhd->creation_time_ = time(NULL);
    mvhd->timescale_ = 1000;
    mvhd->duration_ = 0;
    mvhd->next_track_ID_ = audio_track_ID_ + 1;
    video_moov_->add_box(mvhd);
    {
        //trak
        auto trak = std::make_shared<TrakBox>();
        video_moov_->add_box(trak);
        // tkhd
        auto tkhd = std::make_shared<TkhdBox>(0, 0x03);
        tkhd->track_ID_ = audio_track_ID_;
        tkhd->duration_ = 0;
        tkhd->volume_ = 0x0100;
        trak->set_tkhd(tkhd);
        // mdia
        auto mdia = std::make_shared<MdiaBox>();
        trak->set_mdia(mdia);
        {
            // mdhd
            auto mdhd = std::make_shared<MdhdBox>();
            mdhd->timescale_ = 1000;
            mdhd->duration_ = 0;
            mdhd->set_language0('u');
            mdhd->set_language1('n');
            mdhd->set_language2('d');
            mdia->set_mdhd(mdhd);
            // hdlr
            auto hdlr = std::make_shared<HdlrBox>();
            hdlr->handler_type_ = HANDLER_TYPE_SOUN;
            hdlr->name_ = "SoundHandler";
            mdia->set_hdlr(hdlr);
            // minf
            auto minf = std::make_shared<MinfBox>();
            {
                auto vmhd = std::make_shared<VmhdBox>();
                minf->add_box(vmhd);
                auto dinf = std::make_shared<DinfBox>();
                minf->add_box(dinf);
                {
                    auto dref = std::make_shared<DrefBox>();
                    auto url = std::make_shared<UrlBox>();
                    dref->entries_.push_back(url);
                    dinf->set_dref(dref);
                }

                auto stbl = std::make_shared<StblBox>();
                {
                    
                    auto stts = std::make_shared<SttsBox>();
                    stbl->add_box(stts);
                    auto stsc = std::make_shared<StscBox>();
                    stbl->add_box(stsc);
                    auto stsz = std::make_shared<StszBox>();
                    stbl->add_box(stsz);
                    auto stco = std::make_shared<StcoBox>();
                    stbl->add_box(stco);

                    auto stsd = std::make_shared<StsdBox>();
                    {
                        AACCodec *aac_codec = ((AACCodec*)audio_codec_.get());
                        auto audio_specific_config = aac_codec->get_audio_specific_config();
                        auto mp4a = std::make_shared<AudioSampleEntry>(BOX_TYPE_MP4A);
                        mp4a->data_reference_index_ = 1;
                        mp4a->sample_rate_ = (audio_specific_config->sampling_frequency << 16);
                        mp4a->sample_size_ = 16;//aac 固定位深为16
                        mp4a->channel_count_ = audio_specific_config->channel_configuration;
                        
                        AudioTagHeader header;
                        auto payload = audio_pkt->get_using_data();
                        int32_t header_consumed = header.decode((uint8_t*)payload.data(), payload.size());
                        auto esds = std::make_shared<EsdsBox>();
                        mp4a->esds_ = esds;
                        Mp4ES_Descriptor & es = esds->es_;
                        es.ES_ID_ = 0x02;
                        Mp4DecoderConfigDescriptor& desc = es.decConfigDescr_;
                        desc.object_type_indication_ = Mp4ObjectTypeAac;
                        desc.stream_type_ = Mp4StreamTypeAudioStream;

                        desc.dec_specific_info_ = std::make_shared<Mp4DecoderSpecificInfo>();
                        desc.dec_specific_info_->asc_ = std::string(payload.data() + header_consumed, payload.size() - header_consumed);

                        stsd->entries_.push_back(mp4a);
                    }
                    stbl->add_box(stsd);
                    
                }
                minf->add_box(stbl);
            }
            mdia->set_minf(minf);
        }   

    }

    auto mvex = std::make_shared<MvexBox>();
    auto trex = std::make_shared<TrexBox>();
    trex->track_ID_ = audio_track_ID_;
    trex->default_sample_description_index_ = 1;
    mvex->set_trex(trex);
    video_moov_->add_box(mvex);
    
    video_moov_->size();
    n = NetBuffer(init_audio_mp4_seg_->alloc_buffer(video_moov_->size()));
    video_moov_->encode(n);
    auto used_buf = init_audio_mp4_seg_->get_used_buf();
    of.write(used_buf.data(), used_buf.size());
    of.close();
    init_video_mp4_seg_->set_filename("audio-init.m4s");
    mp4_media_source_->on_audio_init_segment(init_audio_mp4_seg_);
    return true;
}

bool RtmpToMp4::process_h265_packet(std::shared_ptr<RtmpMessage> video_pkt) {
    VideoTagHeader header;
    auto payload = video_pkt->get_using_data();
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

    return true;
}

int32_t RtmpToMp4::get_nalus(uint8_t *data, int32_t len, std::list<std::string_view> & nalus) {
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

bool RtmpToMp4::on_audio_packet(std::shared_ptr<RtmpMessage> audio_pkt) {
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

bool RtmpToMp4::process_aac_packet(std::shared_ptr<RtmpMessage> audio_pkt) {
    AudioTagHeader header;
    auto payload = audio_pkt->get_using_data();
    int32_t header_consumed = header.decode((uint8_t*)payload.data(), payload.size());
    if (header_consumed < 0) {
        return false;
    }
    
    AACCodec *aac_codec = ((AACCodec*)audio_codec_.get());
    if (header.is_seq_header()) {// 关键帧索引
        audio_ready_ = true;
        audio_header_ = audio_pkt;
        // 解析aac configuration header
        std::shared_ptr<AudioSpecificConfig> audio_config = std::make_shared<AudioSpecificConfig>();
        int32_t consumed = audio_config->parse((uint8_t*)payload.data() + header_consumed, payload.size() - header_consumed);
        if (consumed < 0) {
            CORE_ERROR("parse aac audio header failed, ret:{}", consumed);
            return false;
        }

        aac_codec->set_audio_specific_config(audio_config);
        generate_audio_init_seg(audio_pkt);
        return true;
    }

    if (!audio_ready_) {
        return false;
    }

    if (audio_pkts_.size() >= 2) {
        int64_t duration = audio_pkts_[audio_pkts_.size() - 1]->timestamp_ - audio_pkts_[0]->timestamp_;
        if (publish_app_->can_reap_mp4(false, duration, audio_bytes_) || video_reaped_) {
            video_reaped_ = false;
            reap_audio_seg(audio_pkt->timestamp_);
            audio_pkts_.clear();
            audio_bytes_ = 0;
            mp4_media_source_->on_audio_mp4_segment(audio_data_mp4_seg_);
        }
    }

    audio_pkts_.push_back(audio_pkt);
    audio_bytes_ += payload.size() - header_consumed;

    return true;
}

void RtmpToMp4::reap_audio_seg(int64_t dts) {
    audio_data_mp4_seg_ = std::make_shared<Mp4Segment>();
    std::stringstream ss;
    ss << "audio-" << audio_seq_no_ << ".m4s";
    std::ofstream of(ss.str(), std::ios::out|std::ios::binary);
    StypBox styp;
    styp.major_brand_ = Mp4BoxBrandMSDH;
    styp.minor_version_ = 0;
    styp.compatible_brands_.push_back(Mp4BoxBrandMSDH);
    styp.compatible_brands_.push_back(Mp4BoxBrandMSIX);
    size_t s = styp.size();
    NetBuffer n(audio_data_mp4_seg_->alloc_buffer(s));
    styp.encode(n);

    auto sidx = std::make_shared<SidxBox>();
    sidx->version_ = 1;
    sidx->reference_id_ = 1;
    sidx->timescale_ = 1000;
    sidx->earliest_presentation_time_ = audio_pkts_[0]->timestamp_;
    auto duration = audio_pkts_[audio_pkts_.size() - 1]->timestamp_ - sidx->earliest_presentation_time_;

    SegmentIndexEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.subsegment_duration = duration;
    entry.starts_with_SAP = 1;
    sidx->entries.push_back(entry);

    auto moof = std::make_shared<MoofBox>();

    auto mfhd = std::make_shared<MfhdBox>();
    mfhd->sequence_number_ = audio_seq_no_;
    moof->mfhd_ = mfhd;

    auto traf = std::make_shared<TrafBox>();
    moof->traf_ = traf;

    auto tfhd = std::make_shared<TfhdBox>(0);
    tfhd->track_id_ = audio_track_ID_;
    tfhd->flags_ = TfhdFlagsDefaultBaseIsMoof;
    traf->tfhd_ = tfhd;

    auto tfdt = std::make_shared<TfdtBox>();
    tfdt->base_media_decode_time = 1;//in ms
    tfdt->version_ = 1;
    traf->tfdt_ = tfdt;

    auto trun = std::make_shared<TrunBox>(0);
    trun->flags_ = TrunFlagsDataOffset | 
                   TrunFlagsSampleDuration | 
                   TrunFlagsSampleSize | 
                   TrunFlagsSampleFlag | 
                   TrunFlagsSampleCtsOffset;
    traf->trun_ = trun;
    trun->entries_.reserve(audio_pkts_.size());
    std::shared_ptr<RtmpMessage> prev_pkt;
    int64_t mdat_bytes = 0;
    for (auto it = audio_pkts_.begin(); it != audio_pkts_.end(); it++) {
        TrunEntry te(trun.get());
        if (!prev_pkt) {
            prev_pkt = *it;
            te.sample_flags_ = 0x02000000;
        } else {
            te.sample_flags_ = 0x01000000;
        }

        auto it_next = it + 1;
        if (it_next == audio_pkts_.end()) {
            te.sample_duration_ = dts - (*it)->timestamp_;
        } else {
            te.sample_duration_ = (*it_next)->timestamp_ - (*it)->timestamp_;
        }
        
        auto payload = (*it)->get_using_data();
        AudioTagHeader header;
        int32_t header_consumed = header.decode((uint8_t*)payload.data(), payload.size());
        te.sample_size_ = payload.size() - header_consumed;
        te.sample_composition_time_offset_ = 0;
        if (te.sample_composition_time_offset_ < 0) {
            trun->version_ = 1;
        }
        mdat_bytes += te.sample_size_;
        trun->entries_.push_back(te);
    }

    auto mdat = std::make_shared<MdatBox>();
    int64_t moof_bytes = moof->size();
    trun->data_offset_ = (int32_t)(moof_bytes + mdat->size());
    // Update the size of sidx.
    SegmentIndexEntry* e = &sidx->entries[0];
    e->referenced_size = moof_bytes +  mdat->size() + mdat_bytes;
    s = sidx->size();
    n = NetBuffer(audio_data_mp4_seg_->alloc_buffer(s));
    sidx->encode(n);

    s = moof->size();
    n = NetBuffer(audio_data_mp4_seg_->alloc_buffer(s));
    moof->encode(n);

    {//写mdat
        s = mdat->size();
        n = NetBuffer(audio_data_mp4_seg_->alloc_buffer(s));
        mdat->encode(n);
        for (auto it = audio_pkts_.begin(); it != audio_pkts_.end(); it++) {
            auto payload = (*it)->get_using_data();
            AudioTagHeader header;
            int32_t header_consumed = header.decode((uint8_t*)payload.data(), payload.size());
            auto frame_bytes = payload.size() - header_consumed;
            n = NetBuffer(audio_data_mp4_seg_->alloc_buffer(frame_bytes));
            memcpy((char*)n.get_curr_buf().data(), payload.data() + header_consumed, frame_bytes);
        }
    }
    audio_data_mp4_seg_->set_seqno(audio_seq_no_);
    audio_data_mp4_seg_->update_timestamp(audio_pkts_[0]->timestamp_, audio_pkts_[audio_pkts_.size()-1]->timestamp_);
    audio_data_mp4_seg_->set_filename("audio-" + std::to_string(audio_seq_no_++) + ".m4s");
    auto used_buf = audio_data_mp4_seg_->get_used_buf();
    of.write(used_buf.data(), used_buf.size());
    of.close();
}

bool RtmpToMp4::process_mp3_packet(std::shared_ptr<RtmpMessage> audio_pkt) {
    (void)audio_pkt;
    return true;
}

void RtmpToMp4::close() {
    if (closed_.test_and_set(std::memory_order_acquire)) {
        return;
    }

    auto self(shared_from_this());
    boost::asio::co_spawn(worker_->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        check_closable_timer_.cancel();
        co_await wg_.wait();
        
        if (mp4_media_source_) {
            mp4_media_source_->close();
            mp4_media_source_ = nullptr;
        }

        auto origin_source = origin_source_.lock();
        if (rtmp_media_sink_) {
            rtmp_media_sink_->on_rtmp_message({});
            rtmp_media_sink_->close();
            if (origin_source) {
                origin_source->remove_media_sink(rtmp_media_sink_);
            }
            rtmp_media_sink_ = nullptr;
        }

        if (origin_source) {
            origin_source->remove_bridge(shared_from_this());
        }
        co_return;
    }, boost::asio::detached);
}