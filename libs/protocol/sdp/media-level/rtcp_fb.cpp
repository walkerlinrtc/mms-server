#include <sstream>
#include <vector>

#include <boost/algorithm/string.hpp>
#include "base/utils/utils.h"
#include "rtcp_fb.h"
using namespace mms;

std::string RtcpFb::prefix = "a=rtcp-fb:";
bool RtcpFb::is_my_prefix(const std::string & line) {
    return boost::starts_with(line, prefix);
}

bool RtcpFb::parse(const std::string & line) {
    std::string::size_type end_pos = line.rfind("\r");
    if (end_pos == std::string::npos) {
        end_pos = line.size() - 1;
    }
    std::string valid_string = line.substr(prefix.size(), end_pos - prefix.size());
    std::vector<std::string> vs;
    boost::split(vs, valid_string, boost::is_any_of(" "));
    if (vs.size() < 2) {
        return false;
    }

    if (vs[0] == "*") {
        pt_ = -1;
    } else {
        pt_ = std::atoi(vs[0].c_str());
    }

    fb_val_ = vs[1];
    if (vs.size() >= 3) {
        fb_param_ = vs[2];
    }

    return true;
}

std::string RtcpFb::to_string() const {
    std::ostringstream oss;
    oss << prefix;
    if (pt_ == -1) {
        oss << "*";
    } else {
        oss << pt_;
    }
    oss << " " << fb_val_;
    if (!fb_param_.empty()) {
        oss << " " << fb_param_;
    }
    oss << std::endl;
    return oss.str();
}