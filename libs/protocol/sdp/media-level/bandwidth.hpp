#pragma once
#include <string>
#include <cstdint>
namespace mms {
// 5.8.  Bandwidth ("b=")
//       b=<bwtype>:<bandwidth>
// This OPTIONAL field denotes the proposed bandwidth to be used by the
//    session or media.  The <bwtype> is an alphanumeric modifier giving
//    the meaning of the <bandwidth> figure.  Two values are defined in
//    this specification, but other values MAY be registered in the future
//    (see Section 8 and [21], [25]):
struct BandWidth {
public:
    static std::string prefix;
    std::string bw_type;
    uint32_t bandwidth = 0;
public:
    bool parse(const std::string & line);
    std::string to_string() const;

    const std::string & get_bw_type() const {
        return bw_type;
    }

    void set_bw_type(const std::string & v) {
        bw_type = v;
    }

    uint32_t get_value() const {
        return bandwidth;
    }

    void set_value(uint32_t v) {
        bandwidth = v;
    }
};
};