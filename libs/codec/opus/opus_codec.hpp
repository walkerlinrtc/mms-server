#pragma once
#include <memory>
#include "../codec.hpp"
#include "protocol/rtp/rtp_au.h"
#include "opus/opus.h"
namespace mms {
class MediaSdp;
class Payload;

class OpusCodec : public Codec {
public:
    OpusCodec() : Codec(CODEC_OPUS, "OPUS") {

    }

    static std::shared_ptr<OpusCodec> create_from_sdp(const MediaSdp & media_sdp, const Payload & payload);
    std::shared_ptr<OpusDecoder> create_decoder(int32_t fs, int32_t channels);

    int32_t getFs() {
        return fs_;
    }

    int32_t get_channels() {
        return channels_;
    }

    std::shared_ptr<Payload> get_payload();
private:
    std::shared_ptr<OpusDecoder> decoder_;
    int32_t fs_;
    int32_t channels_;
    std::shared_ptr<Payload> sdp_payload_;
};

};