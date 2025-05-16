#pragma once
#include <memory>
#include "../codec.hpp"
#include "base/utils/bit_stream.hpp"
#include "av1_define.hpp"

namespace mms {
class MediaSdp;
class Payload;

class AV1Codec : public Codec {
public:
    AV1Codec() : Codec(CODEC_AV1, "AV1") {

    }

    static std::shared_ptr<AV1Codec> create_from_sdp(const MediaSdp & media_sdp, const Payload & payload);
    static std::shared_ptr<AV1Codec> create_from_av1_param(AV1SequenceParameters & av1_param);
    void set_av1_param(AV1SequenceParameters & av1_param) {
        avc_param_ = av1_param;
        avc_param_valid_ = true;
    }

    AV1SequenceParameters & get_av1_param();
private:
    bool gen_av1_param();
    bool avc_param_valid_ = false;
    AV1SequenceParameters avc_param_;
};
};