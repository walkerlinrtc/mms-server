#pragma once
#include "full_box.h"
namespace mms {
class NetBuffer;
// 8.4.5.3 Sound Media Header Box (smhd)
// ISO_IEC_14496-12-base-format-2012.pdf, page 39
// The sound media header contains general presentation information, independent of the coding, for audio
// media. This header is used for all tracks containing audio.
class SmhdBox : public FullBox {
public:
    SmhdBox();
    virtual ~SmhdBox();
public:
    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
public:
    // A fixed-point 8.8 number that places mono audio tracks in a stereo space; 0 is centre (the
    // normal value); full left is -1.0 and full right is 1.0.
    int16_t balance;
    uint16_t reserved;
};
};