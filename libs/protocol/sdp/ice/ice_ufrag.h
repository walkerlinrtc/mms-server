#pragma once
#include <string>
// 15.4.  "ice-ufrag" and "ice-pwd" Attributes

//    The "ice-ufrag" and "ice-pwd" attributes convey the username fragment
//    and password used by ICE for message integrity.  Their syntax is:

//    ice-pwd-att           = "ice-pwd" ":" password
//    ice-ufrag-att         = "ice-ufrag" ":" ufrag
//    password              = 22*256ice-char
//    ufrag                 = 4*256ice-char

//    The "ice-pwd" and "ice-ufrag" attributes can appear at either the
//    session-level or media-level.  When present in both, the value in the
//    media-level takes precedence.  Thus, the value at the session-level
//    is effectively a default that applies to all media streams, unless
//    overridden by a media-level value.  Whether present at the session or
//    media-level, there MUST be an ice-pwd and ice-ufrag attribute for
//    each media stream.  If two media streams have identical ice-ufrag's,
//    they MUST have identical ice-pwd's.

//    The ice-ufrag and ice-pwd attributes MUST be chosen randomly at the
//    beginning of a session.  The ice-ufrag attribute MUST contain at
//    least 24 bits of randomness, and the ice-pwd attribute MUST contain
//    at least 128 bits of randomness.  This means that the ice-ufrag
//    attribute will be at least 4 characters long, and the ice-pwd at
//    least 22 characters long, since the grammar for these attributes
//    allows for 6 bits of randomness per character.  The attributes MAY be
//    longer than 4 and 22 characters, respectively, of course, up to 256
//    characters.  The upper limit allows for buffer sizing in
//    implementations.  Its large upper limit allows for increased amounts
//    of randomness to be added over time.
namespace mms {
struct IceUfrag {
public:
    static std::string prefix;
    IceUfrag() = default;
    IceUfrag(const std::string & u) : ufrag_(u) {

    }

    bool parse(const std::string & line);
    const std::string & getUfrag() const {
        return ufrag_;
    }

    void setUfrag(const std::string & val) {
        ufrag_ = val;
    }

    std::string to_string() const;
protected:
    std::string ufrag_;
};
};