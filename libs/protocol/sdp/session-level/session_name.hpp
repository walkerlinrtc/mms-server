#pragma once
#include <string>
// 5.3.  Session Name ("s=")
// s=<session name>
namespace mms {
struct SessionName {
public:
    static std::string prefix;
    SessionName() = default;
    SessionName(const std::string session_name) {
        session_name_ = session_name;
    }
    
    bool parse(const std::string & line);

    const std::string & get_session_name() const {
        return session_name_;
    }

    void set_session_name(const std::string & val) {
        session_name_ = val;
    }

    std::string to_string() const;
public:
    //    The "s=" field is the textual session name.  There MUST be one and
    //    only one "s=" field per session description.  The "s=" field MUST NOT
    //    be empty and SHOULD contain ISO 10646 characters (but see also the
    //    "a=charset" attribute).  If a session has no meaningful name, the
    //    value "s= " SHOULD be used (i.e., a single space as the session
    //    name).
    std::string session_name_;
};
};