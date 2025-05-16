#pragma once
#include <vector>
#include "protocol/sdp/attribute/common/attribute.hpp"
namespace mms {
struct BundleAttr : public Attribute {
public:
    static std::string prefix;
    BundleAttr() = default;
    BundleAttr(std::initializer_list<std::string> mids) : mids_(mids) {

    }
    virtual bool parse(const std::string & line);

    const std::vector<std::string> & get_mids() const {
        return mids_;
    }

    void add_mid(const std::string & mid) {
        mids_.push_back(mid);
    }

    std::string to_string() const;
private:
    std::vector<std::string> mids_;
};
};