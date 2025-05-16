#include <boost/algorithm/string.hpp>
#include <iostream>
#include <string_view>

#include "hevc_codec.hpp"
#include "base/utils/utils.h"
#include "protocol/sdp/media-level/media_sdp.hpp"
#include "hevc_hvcc.hpp"

#include "spdlog/spdlog.h"

using namespace mms;
Json::Value HevcCodec::to_json() {
    Json::Value v;
    v["name"] = codec_name_;
    uint32_t w, h;
    if (get_wh(w, h)) {
        v["width"] = w;
        v["height"] = h;
    }

    return v;
}

bool HevcCodec::is_ready() {
    return sps_nalu_.size() > 0 && pps_nalu_.size() > 0 && vps_nalu_.size() > 0;
}

//a=fmtp:98 profile-level-id=42A01E; packetization-mode=0;
//  sprop-parameter-sets=&lt;parameter sets data#0&gt;
std::shared_ptr<HevcCodec> HevcCodec::create_from_sdp(const MediaSdp & media_sdp, const Payload & payload) {
    ((void)media_sdp);
    const auto & fmtps = payload.get_fmtps();
    auto pt = payload.get_pt();
    auto it_fmtp = fmtps.find(pt);
    if (it_fmtp == fmtps.end()) {
        // CORE_ERROR("could not find fmtp for pt:{}", pt);
        return nullptr;
    }

    std::shared_ptr<HevcCodec> codec = std::make_shared<HevcCodec>();
    auto vps = it_fmtp->second.get_param("sprop-vps");
    if (!vps.empty()) {
        if (!Utils::decode_base64(vps, codec->vps_nalu_)) {
            return nullptr;
        }
    }

    auto sps = it_fmtp->second.get_param("sprop-sps");
    if (!sps.empty()) {
        if (!Utils::decode_base64(sps, codec->sps_nalu_)) {
            return nullptr;
        }
    }

    auto pps = it_fmtp->second.get_param("sprop-pps");
    if (!vps.empty()) {
        if (!Utils::decode_base64(pps, codec->pps_nalu_)) {
            return nullptr;
        }
    }
    codec->ready_ = true;
    
    return codec;
}

std::shared_ptr<HevcCodec> HevcCodec::create_from_hvcc(HEVCDecoderConfigurationRecord & hvcc_configuration) {
    ((void)hvcc_configuration);
    std::shared_ptr<HevcCodec> codec = std::make_shared<HevcCodec>();
    // codec->sps_nalu_ = hvcc_configuration.get_sps();
    // codec->pps_nalu_ = hvcc_configuration.get_pps();
    return codec;
}

void HevcCodec::set_hevc_configuration(HEVCDecoderConfigurationRecord & hvcc_configuration) {
    hvcc_configuration_ = hvcc_configuration;
    hevc_configuration_valid_ = true;
    if (!hvcc_configuration_.vps_nalu_.empty()) {
        vps_nalu_ = hvcc_configuration_.vps_nalu_;
    }

    if (!hvcc_configuration_.sps_nalu_.empty()) {
        sps_nalu_ = hvcc_configuration_.sps_nalu_;
    }

    if (!hvcc_configuration_.pps_nalu_.empty()) {
        pps_nalu_ = hvcc_configuration_.pps_nalu_;
    }
}

HEVCDecoderConfigurationRecord & HevcCodec::get_hevc_configuration() {
    if (!hevc_configuration_valid_ || hevc_configuration_changed_) {
        // CORE_ERROR("gen_hevc_decoder_configuration_record");
        if (!gen_hevc_decoder_configuration_record()) {
            // CORE_ERROR("genHEVCDecoderConfigurationRecord failed");
        } else {
            hevc_configuration_valid_ = true;
        }
        hevc_configuration_changed_ = false;
    }
    return hvcc_configuration_;
}

bool HevcCodec::gen_hevc_decoder_configuration_record() {
    if (sps_nalu_.size() <= 0 || pps_nalu_.size() <= 0 || vps_nalu_.size() <= 0) {
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
        int32_t width  = (int)(sps_.pic_width_in_mbs_minus1 + 1) * 16;
        int32_t height = (int)(sps_.pic_height_in_map_units_minus1 + 1) * 16;
        if (sps_.frame_cropping_flag) {
            width = width - sps_.frame_crop_left_offset*2 - sps_.frame_crop_right_offset*2;
            height = ((2 - sps_.frame_mbs_only_flag)* height) - (sps_.frame_crop_top_offset * 2) - (sps_.frame_crop_bottom_offset * 2);
        }
        // CORE_ERROR("width={}, height={}", width, height);
        sps_pps_parsed_ = true;
    }

    #define MAX_SPATIAL_SEGMENTATION 4096 // max. value of u(12) field
    hvcc_configuration_.version = 1;
    hvcc_configuration_.general_profile_idc = sps_.profile_idc;
    hvcc_configuration_.general_level_idc = 93;//sps_.level_idc;
    hvcc_configuration_.general_profile_compatibility_flags = 0xffffffff;
    memset(hvcc_configuration_.general_constraint_indicator_flags, 0xff, 6);//  = 0xffffffffffff;
    hvcc_configuration_.min_spatial_segmentation_idc = (uint8_t)(MAX_SPATIAL_SEGMENTATION + 1);
    hvcc_configuration_.lengthSizeMinusOne = 3;//我们固定为最大的位数
    hvcc_configuration_.add_nalu(H265_TYPE(vps_nalu_.data()[0]), vps_nalu_);
    hvcc_configuration_.add_nalu(H265_TYPE(sps_nalu_.data()[0]), sps_nalu_);
    hvcc_configuration_.add_nalu(H265_TYPE(pps_nalu_.data()[0]), pps_nalu_);
    if (sei_prefix_nalu_.size() > 0) {
        hvcc_configuration_.add_nalu(H265_TYPE(sei_prefix_nalu_.data()[0]), sei_prefix_nalu_);
    }
    
    // CORE_DEBUG("genHEVCDecoderConfigurationRecord succeed!");
    return true;
}

bool HevcCodec::set_sps_pps_vps(const std::string & sps, const std::string & pps, const std::string & vps) {
    sps_nalu_ = sps;
    pps_nalu_ = pps;
    vps_nalu_ = vps;
    ready_ = true;
    return true;
}

void HevcCodec::set_sps(const std::string & sps) {
    if (sps_nalu_.size() != sps.size() || (sps_nalu_.size() > 0 && memcmp(sps_nalu_.data(), sps.data(), sps.size()) != 0)) {
        // CORE_ERROR("sps changed");
    }
    sps_nalu_ = sps;
    hevc_configuration_changed_ = true;
}

void HevcCodec::set_pps(const std::string & pps) {
    if (pps_nalu_.size() != pps.size() || (pps_nalu_.size() > 0 && memcmp(pps_nalu_.data(), pps.data(), pps.size()) != 0)) {
        // CORE_ERROR("pps changed");
    }
    pps_nalu_ = pps;
    hevc_configuration_changed_ = true;
}

void HevcCodec::set_vps(const std::string & vps) {
    if (vps_nalu_.size() != vps.size() || (vps_nalu_.size() > 0 && memcmp(vps_nalu_.data(), vps.data(), vps.size()) != 0)) {
        // CORE_ERROR("vps changed");
    }
    vps_nalu_ = vps;
    hevc_configuration_changed_ = true;
}

void HevcCodec::set_sei_prefix(const std::string & sei_prefix) {
    sei_prefix_nalu_ = sei_prefix;
}

void HevcCodec::deemulation_prevention(const std::string_view & input, std::string & output) {
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
            output[i] = input[i];
            i++;
            pos++;
        }
    }
}

bool HevcCodec::get_wh(uint32_t & w, uint32_t & h) {
    if (!sps_pps_parsed_) {
        // sps处理
        std::string_view sps_buf(sps_nalu_.data(), sps_nalu_.size());
        sps_buf.remove_prefix(1);//去掉nalu头部
        deemulation_prevention(sps_buf, sps_rbsp_);
        std::string_view sps_rbsp_vw(sps_rbsp_.data(), sps_rbsp_.size());
        BitStream bit_stream(sps_rbsp_vw);
        // if (!sps_.parse(bit_stream)) {
        //     return false;
        // }

        sps_pps_parsed_ = true;
    }

    // int32_t width  = (int)(sps_.pic_width_in_mbs_minus1 + 1) * 16;
    // int32_t height = (int)(sps_.pic_height_in_map_units_minus1 + 1) * 16;
    // if (sps_.frame_cropping_flag) {
    //     width = width - sps_.frame_crop_left_offset*2 - sps_.frame_crop_right_offset*2;
    //     height = ((2 - sps_.frame_mbs_only_flag)* height) - (sps_.frame_crop_top_offset * 2) - (sps_.frame_crop_bottom_offset * 2);
    // }
    // width_ = width;
    // height_ = height;

    w = width_;
    h = height_;
    return true;
}

bool HevcCodec::get_fps(double & fps) {
    ((void)fps);
    // if (!sps_pps_parsed_) {
    //     // sps处理
    //     std::string_view sps_buf(sps_nalu_.data(), sps_nalu_.size());
    //     sps_buf.remove_prefix(1);//去掉nalu头部
    //     deemulation_prevention(sps_buf, sps_rbsp_);
    //     std::string_view sps_rbsp_vw(sps_rbsp_.data(), sps_rbsp_.size());
    //     BitStream bit_stream(sps_rbsp_vw);
    //     if (!sps_.parse(bit_stream)) {
    //         CORE_ERROR("parse sps failed");
    //         return false;
    //     }

    //     sps_pps_parsed_ = true;
    // }

    // if (sps_.vui_parameters_present_flag != 1) {
    //     CORE_DEBUG("no vui parameters present, could not get fps");
    //     return false;
    // }
    
    // if (sps_.vui_parameters.timing_info_present_flag != 1) {
    //     CORE_DEBUG("no timing_info_present_flag present, could not get fps");
    //     return false;
    // }

    // if (sps_.vui_parameters.num_units_in_tick == 0) {
    //     return false;
    // }

    // fps_ = (double)sps_.vui_parameters.time_scale/sps_.vui_parameters.num_units_in_tick;
    // fps = fps_;
    return true;
}