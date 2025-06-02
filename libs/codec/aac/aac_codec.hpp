#pragma once
#include <memory>
#include "../codec.hpp"
#include "mpeg4_aac.hpp"
#include "protocol/rtp/rtp_au.h"
#include "json/json.h"

namespace mms {
class MediaSdp;
class Payload;
class AUConfig;
class AudioSpecificConfig;

class AACCodec : public Codec {
public:
    AACCodec() : Codec(CODEC_AAC, "AAC") {

    }

    virtual ~AACCodec() {

    }

    static std::shared_ptr<AACCodec> create_from_sdp(const MediaSdp & media_sdp, const Payload & payload);
    Json::Value to_json();
public:
    void set_audio_specific_config(std::shared_ptr<AudioSpecificConfig> audio_specific_config);
    std::shared_ptr<AudioSpecificConfig> get_audio_specific_config();
    std::shared_ptr<AUConfig> get_au_config();
    std::shared_ptr<Payload> get_payload();
private:
    std::shared_ptr<AudioSpecificConfig> audio_specific_config_;
    std::shared_ptr<AUConfig> au_config_;
    std::shared_ptr<Payload> sdp_payload_;

    int channel_;//声道数
    int sample_rate_;//采样率
};

};