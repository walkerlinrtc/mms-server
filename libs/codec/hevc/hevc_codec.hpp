/*
 * @Author: jbl19860422
 * @Date: 2023-11-09 19:18:47
 * @LastEditTime: 2023-11-09 20:47:55
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\core\codec\hevc\hevc_codec.hpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include <memory>
#include "../codec.hpp"
#include "hevc_sps.hpp"
#include "hevc_pps.hpp"
#include "base/utils/bit_stream.hpp"
#include "hevc_hvcc.hpp"
#include "hevc_define.hpp"
#include "json/json.h"
#include "codec/h264/h264_sps.hpp"

namespace mms {
class MediaSdp;
class Payload;
class HEVCDecoderConfigurationRecord;

class HevcCodec : public Codec {
public:
    HevcCodec() : Codec(CODEC_HEVC, "HEVC") {

    }

    Json::Value to_json();
    
    bool is_ready() override;
    bool set_sps_pps_vps(const std::string & sps, const std::string & pps, const std::string & vps);
    static std::shared_ptr<HevcCodec> create_from_sdp(const MediaSdp & media_sdp, const Payload & payload);
    static std::shared_ptr<HevcCodec> create_from_hvcc(HEVCDecoderConfigurationRecord & hvcc_configuration);

    const std::string & get_sps_nalu() const {
        return sps_nalu_;
    }

    const std::string & get_pps_nalu() const {
        return pps_nalu_;
    }

    const std::string & get_vps_nalu() const {
        return vps_nalu_;
    }

    const std::string & get_sps_rbsp() const {
        return sps_rbsp_;
    }

    const std::string & get_pps_rbsp() const {
        return pps_rbsp_;
    }

    const std::string & get_vps() const {
        return vps_rbsp_;
    }

    void set_sps(const std::string & sps);
    void set_pps(const std::string & pps);
    void set_vps(const std::string & vps);
    void set_sei_prefix(const std::string & sei_prefix);

    bool get_wh(uint32_t & w, uint32_t & h);
    bool get_fps(double & fps);

    void set_hevc_configuration(HEVCDecoderConfigurationRecord & hvcc_configuration);
    HEVCDecoderConfigurationRecord & get_hevc_configuration();
private:
    bool gen_hevc_decoder_configuration_record();
    void deemulation_prevention(const std::string_view & input, std::string & output);//去除竞争字段
    bool sps_pps_parsed_ = false;
    std::string sps_nalu_; 
    std::string pps_nalu_;
    std::string vps_nalu_;
    std::string sei_prefix_nalu_;

    std::string sps_rbsp_;
    std::string pps_rbsp_;
    std::string vps_rbsp_;

    // h264_sps_t sps_;

    uint32_t width_ = 0;
    uint32_t height_ = 0;
    double fps_ = 0;

    bool hevc_configuration_valid_ = false;
    bool hevc_configuration_changed_ = false;
    h264_sps_t sps_;
    HEVCDecoderConfigurationRecord hvcc_configuration_;
};
};