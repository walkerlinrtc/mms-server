#include "spdlog/spdlog.h"

#include <boost/algorithm/string.hpp>
#include <iostream>

#include "h264_codec.hpp"
#include "base/utils/utils.h"
#include "protocol/sdp/media-level/media_sdp.hpp"
#include "protocol/sdp/media-level/payload.h"

#include "h264_avcc.hpp"
using namespace mms;
//a=fmtp:98 profile-level-id=42A01E; packetization-mode=0;
//  sprop-parameter-sets=&lt;parameter sets data#0&gt;
std::shared_ptr<H264Codec> H264Codec::create_from_sdp(const MediaSdp & media_sdp, const Payload & payload) {
    ((void)media_sdp);
    const auto & fmtps = payload.get_fmtps();
    auto pt = payload.get_pt();
    auto it_fmtp = fmtps.find(pt);
    if (it_fmtp == fmtps.end()) {
        spdlog::debug("could not find fmtp for pt:{}", pt);
        return nullptr;
    }

    std::shared_ptr<H264Codec> codec = std::make_shared<H264Codec>();
    auto sps_pps = it_fmtp->second.get_param("sprop-parameter-sets");
    if (!sps_pps.empty()) {
        std::vector<std::string> vs;
        boost::split(vs, sps_pps, boost::is_any_of(","));
        if (vs.size() != 2) {
            spdlog::debug("sdp's sps pps format error, no comma find ");
            return nullptr;
        }

        if (!Utils::decode_base64(vs[0], codec->sps_nalu_)) {
            spdlog::debug("sdp's sps pps format error, not base64");
            return nullptr;
        }

        if (!Utils::decode_base64(vs[1], codec->pps_nalu_)) {
            spdlog::debug("sdp's sps pps format error, not base64");
            return nullptr;
        }

        codec->ready_ = true;
    }

    codec->sdp_payload_ = std::make_shared<Payload>();
    *(codec->sdp_payload_) = payload;
    return codec;
}

std::shared_ptr<H264Codec> H264Codec::create_from_avcc(AVCDecoderConfigurationRecord & avc_configuration) {
    std::shared_ptr<H264Codec> codec = std::make_shared<H264Codec>();
    codec->sps_nalu_ = avc_configuration.get_sps();
    codec->pps_nalu_ = avc_configuration.get_pps();
    return codec;
}

AVCDecoderConfigurationRecord & H264Codec::get_avc_configuration() {
    if (!avc_configuration_valid_) {
        if (!gen_avc_decoder_configuration_record()) {
            spdlog::debug("gen_avc_decoder_configuration_record failed");
        } else {
            avc_configuration_valid_ = true;
        }
    }
    return avc_configuration_;
}

bool H264Codec::gen_avc_decoder_configuration_record() {
    if (sps_nalu_.size() <= 0 || pps_nalu_.size() <= 0) {
        return false;
    }

    if (!sps_pps_parsed_) {
        // sps处理
        std::string_view sps_buf(sps_nalu_.data(), sps_nalu_.size());
        sps_buf.remove_prefix(1);//去掉nalu头部
        deemulation_prevention(sps_buf, sps_rbsp_);
        std::string_view sps_rbsp_vw(sps_rbsp_.data(), sps_rbsp_.size());
        BitStream bit_stream(sps_rbsp_vw);
        int ret = sps_.parse(bit_stream);
        if (0 != ret) {
            spdlog::debug("parse sps failed, code:{}", ret);
            return false;
        }

        sps_pps_parsed_ = true;
    }

    avc_configuration_.avc_profile = sps_.profile_idc;
    avc_configuration_.avc_level = sps_.level_idc;
    avc_configuration_.nalu_length_size_minus_one = 3;//我们固定为最大的位数
    avc_configuration_.add_sps(sps_nalu_);
    avc_configuration_.add_pps(pps_nalu_);
    return true;
}

std::shared_ptr<Payload> H264Codec::get_payload() {
    return sdp_payload_;
}

bool H264Codec::set_sps_pps(const std::string & sps, const std::string & pps) {
    sps_nalu_ = sps;
    pps_nalu_ = pps;
    ready_ = true;
    return true;
}

void H264Codec::set_sps(const std::string & sps) {
    sps_nalu_ = sps;
    if (sps_nalu_.size() > 0 && pps_nalu_.size() > 0) {
        ready_ = true;
    }
}

void H264Codec::set_pps(const std::string & pps) {
    pps_nalu_ = pps;
    if (sps_nalu_.size() > 0 && pps_nalu_.size() > 0) {
        ready_ = true;
    }
}

void H264Codec::deemulation_prevention(const std::string_view & input, std::string & output) {
    auto size = input.size();
    output.resize(size);
    int pos = 0;
    for (size_t i = 0; i < size - 2; ) {
        int val = (input.at(i)^0x00) + (input.at(i+1)^0x00) + (input.at(i+2)^0x03);
        if (val == 0) {
            output[pos] = 0;
            output[pos + 1] = 0;
            i += 3;
            pos += 2;
        } else {
            output[pos] = input[i];
            i++;
            pos++;
        }
    }
    output[pos++] = input[input.size() - 2];
    output[pos++] = input[input.size() - 1];
    output.resize(pos);
}

bool H264Codec::get_wh(uint32_t & w, uint32_t & h) {
    if (sps_nalu_.size() <= 0 || pps_nalu_.size() <= 0) {
        return false;
    }
    
    if (!sps_pps_parsed_) {
        // sps处理
        std::string_view sps_buf(sps_nalu_.data(), sps_nalu_.size());
        sps_buf.remove_prefix(1);                   //去掉nalu头部
        deemulation_prevention(sps_buf, sps_rbsp_); //去掉防竞争插入字节
        std::string_view sps_rbsp_vw(sps_rbsp_.data(), sps_rbsp_.size());
        BitStream bit_stream(sps_rbsp_vw);
        int ret = sps_.parse(bit_stream);
        if (0 != ret) {
            spdlog::error("parse sps failed, size:{}, {}, ret:{}", sps_rbsp_vw.size(), sps_nalu_.size(), ret);
            return false;
        }

        sps_pps_parsed_ = true;
    }
    
    // 计算长宽
    int32_t width  = (int)(sps_.pic_width_in_mbs_minus1 + 1) * 16;
    int32_t height = (int)(sps_.pic_height_in_map_units_minus1 + 1) * 16;
    if (sps_.frame_mbs_only_flag){
        height *= 2;
    }

    if (sps_.frame_cropping_flag) {
        width = width - sps_.frame_crop_left_offset*2 - sps_.frame_crop_right_offset*2;
        height = height - (sps_.frame_crop_top_offset * 2) - (sps_.frame_crop_bottom_offset * 2);
    }
    
    width_ = width;
    height_ = height;

    w = width_;
    h = height_;
    if (sps_.vui_parameters_present_flag && sps_.vui_parameters.timing_info_present_flag && sps_.vui_parameters.num_units_in_tick > 0) {
        fps_ = (double)sps_.vui_parameters.time_scale/(2*sps_.vui_parameters.num_units_in_tick);
    }
    
    return true;
}

bool H264Codec::get_fps(double & fps) {
    if (sps_nalu_.size() <= 0 || pps_nalu_.size() <= 0) {
        return false;
    }
    
    if (!sps_pps_parsed_) {
        // sps处理
        std::string_view sps_buf(sps_nalu_.data(), sps_nalu_.size());
        sps_buf.remove_prefix(1);//去掉nalu头部
        deemulation_prevention(sps_buf, sps_rbsp_);
        std::string_view sps_rbsp_vw(sps_rbsp_.data(), sps_rbsp_.size());
        BitStream bit_stream(sps_rbsp_vw);
        int ret = sps_.parse(bit_stream);
        if (0 != ret) {
            spdlog::error("parse sps failed, code:{}", ret);
            return false;
        }

        sps_pps_parsed_ = true;
    }

    int32_t width  = (int)(sps_.pic_width_in_mbs_minus1 + 1) * 16;
    int32_t height = (int)(sps_.pic_height_in_map_units_minus1 + 1) * 16;
    if (sps_.frame_mbs_only_flag){
        height *= 2;
    }

    if (sps_.frame_cropping_flag) {
        width = width - sps_.frame_crop_left_offset*2 - sps_.frame_crop_right_offset*2;
        height = height - (sps_.frame_crop_top_offset * 2) - (sps_.frame_crop_bottom_offset * 2);
    }
    
    width_ = width;
    height_ = height;

    if (sps_.vui_parameters_present_flag && sps_.vui_parameters.timing_info_present_flag && sps_.vui_parameters.num_units_in_tick > 0) {
        fps_ = (double)sps_.vui_parameters.time_scale/(2*sps_.vui_parameters.num_units_in_tick);
        fps = fps_;
        return true;
    }

    return false;
}