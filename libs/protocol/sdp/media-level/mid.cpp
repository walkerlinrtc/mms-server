#include <sstream>
#include <iostream>
#include "mid.h"
#include "base/utils/utils.h"
using namespace mms;
std::string MidAttr::prefix = "a=mid:";
bool MidAttr::parse(const std::string & line) {
    std::string::size_type end_pos = line.rfind("\r");
    if (end_pos == std::string::npos) {
        end_pos = line.size() - 1;
    }
    mid_ = line.substr(prefix.size(), end_pos - prefix.size());
    return true;
}

std::string MidAttr::to_string() const {
    std::ostringstream oss;
    oss << prefix << mid_ << std::endl;
    return oss.str();
}

MidAttr::MidAttr(const std::string & mid) {
    mid_ = mid;
}

MidAttr::MidAttr() {
}