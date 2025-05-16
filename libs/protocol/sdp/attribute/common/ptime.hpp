#pragma once
#include <string>
#include <string>
// a=ptime:<packet time>

//     This gives the length of time in milliseconds represented by
//     the media in a packet.  This is probably only meaningful for
//     audio data, but may be used with other media types if it makes
//     sense.  It should not be necessary to know ptime to decode RTP
//     or vat audio, and it is intended as a recommendation for the
//     encoding/packetisation of audio.  It is a media-level
//     attribute, and it is not dependent on charset.
namespace mms {
struct PTimeAttr {
static std::string prefix = "a=ptime:";
public:
    std::string packet_time;
};
};