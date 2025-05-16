#pragma once
/*
a=rtpmap:<payload type> <encoding name>/<clock rate> [/<encoding
    parameters>]

    This attribute maps from an RTP payload type number (as used in
    an "m=" line) to an encoding name denoting the payload format
    to be used.  It also provides information on the clock rate and
    encoding parameters.  It is a media-level attribute that is not
    dependent on charset.



Handley, et al.             Standards Track                    [Page 25]

RFC 4566                          SDP                          July 2006


    Although an RTP profile may make static assignments of payload
    type numbers to payload formats, it is more common for that
    assignment to be done dynamically using "a=rtpmap:" attributes.
    As an example of a static payload type, consider u-law PCM
    coded single-channel audio sampled at 8 kHz.  This is
    completely defined in the RTP Audio/Video profile as payload
    type 0, so there is no need for an "a=rtpmap:" attribute, and
    the media for such a stream sent to UDP port 49232 can be
    specified as:

    m=audio 49232 RTP/AVP 0

    An example of a dynamic payload type is 16-bit linear encoded
    stereo audio sampled at 16 kHz.  If we wish to use the dynamic
    RTP/AVP payload type 98 for this stream, additional information
    is required to decode it:

    m=audio 49232 RTP/AVP 98
    a=rtpmap:98 L16/16000/2

    Up to one rtpmap attribute can be defined for each media format
    specified.  Thus, we might have the following:

    m=audio 49230 RTP/AVP 96 97 98
    a=rtpmap:96 L8/8000
    a=rtpmap:97 L16/8000
    a=rtpmap:98 L16/11025/2

    RTP profiles that specify the use of dynamic payload types MUST
    define the set of valid encoding names and/or a means to
    register encoding names if that profile is to be used with SDP.
    The "RTP/AVP" and "RTP/SAVP" profiles use media subtypes for
    encoding names, under the top-level media type denoted in the
    "m=" line.  In the example above, the media types are
    "audio/l8" and "audio/l16".

    For audio streams, <encoding parameters> indicates the number
    of audio channels.  This parameter is OPTIONAL and may be
    omitted if the number of channels is one, provided that no
    additional parameters are needed.

    For video streams, no encoding parameters are currently
    specified.

    Additional encoding parameters MAY be defined in the future,
    but codec-specific parameters SHOULD NOT be added.  Parameters
    added to an "a=rtpmap:" attribute SHOULD only be those required
    for a session directory to make the choice of appropriate media



Handley, et al.             Standards Track                    [Page 26]

RFC 4566                          SDP                          July 2006


    to participate in a session.  Codec-specific parameters should
    be added in other attributes (for example, "a=fmtp:").

    Note: RTP audio formats typically do not include information
    about the number of samples per packet.  If a non-default (as
    defined in the RTP Audio/Video Profile) packetisation is
    required, the "ptime" attribute is used as given above.
*/
#include <string>
#include <stdint.h>
#include <vector>

namespace mms {
struct Rtpmap {
public:
    static std::string prefix;
    bool parse(const std::string & line);
    uint32_t get_payload_type() const {
        return payload_type;
    }

    void set_payload_type(uint32_t val) {
        payload_type = val;
    }

    const std::string & get_encoding_name() const {
        return encoding_name;
    }

    void set_encoding_name(const std::string & val) {
        encoding_name = val;
    }

    uint32_t get_clock_rate() const {
        return clock_rate;
    }

    void set_clock_rate(uint32_t val) {
        clock_rate = val;
    }

    void add_encoding_param(const std::string & val) {
        encoding_params.push_back(val);
    }

    const std::vector<std::string> & get_encoding_params() const {
        return encoding_params;
    }

    void set_encoding_params(const std::vector<std::string> & params) {
        encoding_params = params;
    }

    std::string to_string() const;
protected:
    uint32_t payload_type;
    std::string encoding_name;
    uint32_t clock_rate;
    std::vector<std::string> encoding_params;
};
};