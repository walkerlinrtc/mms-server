#pragma once
#include <string>
// 5.13.  Attributes ("a=")

//       a=<attribute>
//       a=<attribute>:<value>

//    Attributes are the primary means for extending SDP.  Attributes may
//    be defined to be used as "session-level" attributes, "media-level"
//    attributes, or both.
// A media description may have any number of attributes ("a=" fields)
//    that are media specific.  These are referred to as "media-level"
//    attributes and add information about the media stream.  Attribute
//    fields can also be added before the first media field; these
//    "session-level" attributes convey additional information that applies
//    to the conference as a whole rather than to individual media.

//    Attribute fields may be of two forms:

//    o  A property attribute is simply of the form "a=<flag>".  These are
//       binary attributes, and the presence of the attribute conveys that
//       the attribute is a property of the session.  An example might be
//       "a=recvonly".

//    o  A value attribute is of the form "a=<attribute>:<value>".  For
//       example, a whiteboard could have the value attribute "a=orient:
//       landscape"

//    Attribute interpretation depends on the media tool being invoked.
//    Thus receivers of session descriptions should be configurable in
//    their interpretation of session descriptions in general and of
//    attributes in particular.

//    Attribute names MUST use the US-ASCII subset of ISO-10646/UTF-8.
//    Attribute values are octet strings, and MAY use any octet value
//    except 0x00 (Nul), 0x0A (LF), and 0x0D (CR).  By default, attribute
//    values are to be interpreted as in ISO-10646 character set with UTF-8
//    encoding.  Unlike other text fields, attribute values are NOT
//    normally affected by the "charset" attribute as this would make
//    comparisons against known values problematic.  However, when an
//    attribute is defined, it can be defined to be charset dependent, in
//    which case its value should be interpreted in the session charset
//    rather than in ISO-10646.

//    Attributes MUST be registered with IANA (see Section 8).  If an
//    attribute is received that is not understood, it MUST be ignored by
//    the receiver.
namespace mms {
struct Attribute {
public:
    virtual bool parse(const std::string & line) = 0;
    virtual std::string to_string() const = 0;
};
};