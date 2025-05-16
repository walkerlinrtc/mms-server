#pragma once
// RTCP Feedback Capability Attribute

//    A new payload format-specific SDP attribute is defined to indicate
//    the capability of using RTCP feedback as specified in this document:
//    "a=rtcp-fb".  The "rtcp-fb" attribute MUST only be used as an SDP
//    media attribute and MUST NOT be provided at the session level.  The
//    "rtcp-fb" attribute MUST only be used in media sessions for which the
//    "AVPF" is specified.

//    The "rtcp-fb" attribute SHOULD be used to indicate which RTCP FB
//    messages MAY be used in this media session for the indicated payload
//    type.  A wildcard payload type ("*") MAY be used to indicate that the
//    RTCP feedback attribute applies to all payload types.  If several
//    types of feedback are supported and/or the same feedback shall be



// Ott, et al.                 Standards Track                    [Page 23]

// RFC 4585                        RTP/AVPF                       July 2006


//    specified for a subset of the payload types, several "a=rtcp-fb"
//    lines MUST be used.

//    If no "rtcp-fb" attribute is specified, the RTP receivers MAY send
//    feedback using other suitable RTCP feedback packets as defined for
//    the respective media type.  The RTP receivers MUST NOT rely on the
//    RTP senders reacting to any of the FB messages.  The RTP sender MAY
//    choose to ignore some feedback messages.

//    If one or more "rtcp-fb" attributes are present in a media session
//    description, the RTCP receivers for the media session(s) containing
//    the "rtcp-fb"

//    o  MUST ignore all "rtcp-fb" attributes of which they do not fully
//       understand the semantics (i.e., where they do not understand the
//       meaning of all values in the "a=rtcp-fb" line);

//    o  SHOULD provide feedback information as specified in this document
//       using any of the RTCP feedback packets as specified in one of the
//       "rtcp-fb" attributes for this media session; and

//    o  MUST NOT use other FB messages than those listed in one of the
//       "rtcp-fb" attribute lines.

//    When used in conjunction with the offer/answer model [8], the offerer
//    MAY present a set of these AVPF attributes to its peer.  The answerer
//    MUST remove all attributes it does not understand as well as those it
//    does not support in general or does not wish to use in this
//    particular media session.  The answerer MUST NOT add feedback
//    parameters to the media description and MUST NOT alter values of such
//    parameters.  The answer is binding for the media session, and both
//    offerer and answerer MUST only use feedback mechanisms negotiated in
//    this way.  Both offerer and answerer MAY independently decide to send
//    RTCP FB messages of only a subset of the negotiated feedback
//    mechanisms, but they SHOULD react properly to all types of the
//    negotiated FB messages when received.

//    RTP senders MUST be prepared to receive any kind of RTCP FB messages
//    and MUST silently discard all those RTCP FB messages that they do not
//    understand.

//    The syntax of the "rtcp-fb" attribute is as follows (the feedback
//    types and optional parameters are all case sensitive):

//    (In the following ABNF, fmt, SP, and CRLF are used as defined in
//    [3].)





// Ott, et al.                 Standards Track                    [Page 24]

// RFC 4585                        RTP/AVPF                       July 2006


//       rtcp-fb-syntax = "a=rtcp-fb:" rtcp-fb-pt SP rtcp-fb-val CRLF

//       rtcp-fb-pt         = "*"   ; wildcard: applies to all formats
//                          / fmt   ; as defined in SDP spec

//       rtcp-fb-val        = "ack" rtcp-fb-ack-param
//                          / "nack" rtcp-fb-nack-param
//                          / "trr-int" SP 1*DIGIT
//                          / rtcp-fb-id rtcp-fb-param

//       rtcp-fb-id         = 1*(alpha-numeric / "-" / "_")

//       rtcp-fb-param      = SP "app" [SP byte-string]
//                          / SP token [SP byte-string]
//                          / ; empty

//       rtcp-fb-ack-param  = SP "rpsi"
//                          / SP "app" [SP byte-string]
//                          / SP token [SP byte-string]
//                          / ; empty

//       rtcp-fb-nack-param = SP "pli"
//                          / SP "sli"
//                          / SP "rpsi"
//                          / SP "app" [SP byte-string]
//                          / SP token [SP byte-string]
//                          / ; empty

//    The literals of the above grammar have the following semantics:

//    Feedback type "ack":

//       This feedback type indicates that positive acknowledgements for
//       feedback are supported.

//       The feedback type "ack" MUST only be used if the media session is
//       allowed to operate in ACK mode as defined in Section 3.6.1.

//       Parameters MUST be provided to further distinguish different types
//       of positive acknowledgement feedback.

//       The parameter "rpsi" indicates the use of Reference Picture
//       Selection Indication feedback as defined in Section 6.3.3.








// Ott, et al.                 Standards Track                    [Page 25]

// RFC 4585                        RTP/AVPF                       July 2006


//       If the parameter "app" is specified, this indicates the use of
//       application layer feedback.  In this case, additional parameters
//       following "app" MAY be used to further differentiate various types
//       of application layer feedback.  This document does not define any
//       parameters specific to "app".

//       Further parameters for "ack" MAY be defined in other documents.

//    Feedback type "nack":

//       This feedback type indicates that negative acknowledgements for
//       feedback are supported.

//       The feedback type "nack", without parameters, indicates use of the
//       Generic NACK feedback format as defined in Section 6.2.1.

//       The following three parameters are defined in this document for
//       use with "nack" in conjunction with the media type "video":

//       o "pli" indicates the use of Picture Loss Indication feedback as
//         defined in Section 6.3.1.

//       o "sli" indicates the use of Slice Loss Indication feedback as
//         defined in Section 6.3.2.

//       o "rpsi" indicates the use of Reference Picture Selection
//         Indication feedback as defined in Section 6.3.3.

//       "app" indicates the use of application layer feedback.  Additional
//       parameters after "app" MAY be provided to differentiate different
//       types of application layer feedback.  No parameters specific to
//       "app" are defined in this document.

//       Further parameters for "nack" MAY be defined in other documents.

//    Other feedback types <rtcp-fb-id>:

//       Other documents MAY define additional types of feedback; to keep
//       the grammar extensible for those cases, the rtcp-fb-id is
//       introduced as a placeholder.  A new feedback scheme name MUST to
//       be unique (and thus MUST be registered with IANA).  Along with a
//       new name, its semantics, packet formats (if necessary), and rules
//       for its operation MUST be specified.








// Ott, et al.                 Standards Track                    [Page 26]

// RFC 4585                        RTP/AVPF                       July 2006


//    Regular RTCP minimum interval "trr-int":

//       The attribute "trr-int" is used to specify the minimum interval
//       T_rr_interval between two Regular (full compound) RTCP packets in
//       milliseconds for this media session.  If "trr-int" is not
//       specified, a default value of 0 is assumed.

//    Note that it is assumed that more specific information about
//    application layer feedback (as defined in Section 6.4) will be
//    conveyed as feedback types and parameters defined elsewhere.  Hence,
//    no further provision for any types and parameters is made in this
//    document.

//    Further types of feedback as well as further parameters may be
//    defined in other documents.

//    It is up to the recipients whether or not they send feedback
//    information and up to the sender(s) (how) to make use of feedback
//    provided.

#include <string>
#include <unordered_map>

namespace mms {
struct RtcpFb {
public:
    static std::string prefix;
    static bool is_my_prefix(const std::string & line);
    RtcpFb() = default;
    RtcpFb(int pt, const std::string & fb_val, const std::string & fb_param = "") : pt_(pt), fb_val_(fb_val), fb_param_(fb_param) {

    }

    bool parse(const std::string & line);
    int get_pt() const {
        return pt_;
    }

    void set_pt(int pt) {
        pt_ = pt;
    }

    const std::string & get_fb_val() const {
        return fb_val_;
    }

    void set_fb_val(const std::string & val) {
        fb_val_ = val;
    }

    const std::string & get_fb_param() const {
        return fb_param_;
    }

    void set_fb_param(const std::string & val) {
        fb_param_ = val;
    }

    std::string to_string() const;
public:
    int pt_;//-1代表所有pt，正值代表具体的pt
    std::string fb_val_;
    std::string fb_param_;
};
};