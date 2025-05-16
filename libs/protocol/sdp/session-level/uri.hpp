#pragma once
#include <string>

namespace mms {
// 5.5.  URI ("u=")
//       u=<uri>
//    A URI is a Uniform Resource Identifier as used by WWW clients [7].
//    The URI should be a pointer to additional information about the
//    session.  This field is OPTIONAL, but if it is present it MUST be
//    specified before the first media field.  No more than one URI field
//    is allowed per session description.
class Uri {
public:
    static std::string prefix;
    bool parse(const std::string & line);

    const std::string & get_uri() const {
        return uri;
    }

    void set_uri(const std::string & val) {
        uri = val;
    }

    std::string to_string() const;
public:
    std::string uri;
};
};