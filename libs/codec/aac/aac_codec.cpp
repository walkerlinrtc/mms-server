/*
 * @Author: jbl19860422
 * @Date: 2023-12-17 22:38:25
 * @LastEditTime: 2023-12-28 16:46:46
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\core\codec\aac\aac_codec.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include "spdlog/spdlog.h"

#include "aac_codec.hpp"
#include "protocol/sdp/media-level/media_sdp.hpp"
#include "protocol/sdp/media-level/payload.h"
#include "base/utils/utils.h"
using namespace mms;
/*
 config:
      A hexadecimal representation of an octet string that expresses the
      media payload configuration.  Configuration data is mapped onto
      the hexadecimal octet string in an MSB-first basis.  The first bit
      of the configuration data SHALL be located at the MSB of the first
      octet.  In the last octet, if necessary to achieve octet-
      alignment, up to 7 zero-valued padding bits shall follow the
      configuration data.

      For MPEG-4 Audio streams, config is the audio object type specific
         decoder configuration data AudioSpecificConfig(), as defined in
         ISO/IEC 14496-3.  For Structured Audio, the
         AudioSpecificConfig() may be conveyed by other means, not
         defined by this specification.  If the AudioSpecificConfig() is
         conveyed by other means for Structured Audio, then the config
         MUST be a quoted empty hexadecimal octet string, as follows:
         config="".

         Note that a future mode of using this RTP payload format for
         Structured Audio may define such other means.

      For MPEG-4 Visual streams, config is the MPEG-4 Visual
         configuration information as defined in subclause 6.2.1, Start
         codes of ISO/IEC 14496-2.  The configuration information
         indicated by this parameter SHALL be the same as the
         configuration information in the corresponding MPEG-4 Visual
         stream, except for first-half-vbv-occupancy and latter-half-
         vbv-occupancy, if it exists, which may vary in the repeated
         configuration information inside an MPEG-4 Visual stream (See
         6.2.1 Start codes of ISO/IEC 14496-2).

注：sdp中audio的fmtp中的config字段，就是AudioSpecificConfig，16进制表示
*/
std::shared_ptr<AACCodec> AACCodec::create_from_sdp(const MediaSdp & media_sdp, const Payload & payload) {
    ((void)media_sdp);
    const auto & fmtps = payload.get_fmtps();
    auto pt = payload.get_pt();
    auto it_fmtp = fmtps.find(pt);
    if (it_fmtp == fmtps.end()) {
        return nullptr;
    }

    auto config = it_fmtp->second.get_param("config");
    if (config.empty()) {
        return nullptr;
    }

    std::string bin_config;
    if (!Utils::hex_str_to_bin(config, bin_config)) {
        return nullptr;
    }

    std::shared_ptr<AACCodec> codec = std::make_shared<AACCodec>();
    auto asc = std::make_shared<AudioSpecificConfig>();
    int32_t ret = asc->parse((uint8_t*)bin_config.data(), bin_config.size());
    if (ret <= 0) {
        return nullptr;
    }
    codec->audio_specific_config_ = asc;

    // 获取au header的格式信息
    // sizelength
    auto ssize_length = it_fmtp->second.get_param("sizelength");
    std::shared_ptr<AUConfig> au_config;
    try {
        au_config = std::make_shared<AUConfig>();
        au_config->size_length = std::atoi(ssize_length.c_str());
    } catch (std::exception & e) {
        return nullptr;
    }
    
    auto sindex_length = it_fmtp->second.get_param("indexlength");
    if (!sindex_length.empty()) {
        try {
            au_config->index_length = std::atoi(sindex_length.c_str());
        } catch (std::exception & e) {
            return nullptr;
        }
    }

    auto sindex_delta_length = it_fmtp->second.get_param("indexdeltalength");
    if (!sindex_delta_length.empty()) {
        try {
            au_config->index_delta_length = std::atoi(sindex_delta_length.c_str());
        } catch (std::exception & e) {
            return nullptr;
        }
    }
    codec->au_config_ = au_config;

    codec->ready_ = true;
    codec->sdp_payload_ = std::make_shared<Payload>();
    *(codec->sdp_payload_) = payload;
    return codec;
}

void AACCodec::set_audio_specific_config(std::shared_ptr<AudioSpecificConfig> audio_specific_config) {
    audio_specific_config_ = audio_specific_config;
    ready_ = true;
}

std::shared_ptr<AudioSpecificConfig> AACCodec::get_audio_specific_config() {
    return audio_specific_config_;
}

std::shared_ptr<AUConfig> AACCodec::get_au_config() {
    return au_config_;
}

std::shared_ptr<Payload> AACCodec::get_payload() {
    return sdp_payload_;
}