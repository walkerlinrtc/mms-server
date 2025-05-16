#pragma once
#include <string>
// http://mirrors.nju.edu.cn/rfc/inline-errata/rfc5888.html

namespace mms {
struct MidAttr {
public:
    MidAttr();
    MidAttr(const std::string & mid);
    static std::string prefix;
    bool parse(const std::string & line);

    const std::string & get_mid() const {
        return mid_;
    }

    void set_mid(const std::string & val) {
        mid_ = val;
    }

    std::string to_string() const;
public:
    std::string mid_;
};
};