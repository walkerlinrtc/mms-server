#include <sstream>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>
#include "base/utils/utils.h"
#include "payload.h"
using namespace mms;
std::string Payload::prefix = "a=rtpmap:";
bool Payload::is_my_prefix(const std::string & line) {
    if (boost::starts_with(line, prefix)) {
        return true;
    } 
    return false;
}

bool Payload::parse(const std::string & line) {
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

    payload_type_ = std::atoi(vs[0].c_str());
    std::vector<std::string> tmp;
    boost::split(tmp, vs[1], boost::is_any_of("/"));
    if (tmp.size() < 2) {
        return false;
    }
    encoding_name_ = tmp[0];
    boost::algorithm::to_upper(encoding_name_);
    clock_rate_ = std::atoi(tmp[1].c_str());
    for (size_t i = 2; i < tmp.size(); i++) {
        encoding_params_.emplace_back(tmp[i]);
    }
    return true;
}

bool Payload::parse_rtcp_fb_attr(const std::string & line)
{
    RtcpFb fb;
    if (!fb.parse(line)) {
        return false;
    }

    rtcp_fbs_.emplace_back(fb);
    return true;
}

bool Payload::parse_fmtp_attr(const std::string & line)
{
    Fmtp fmtp;
    if (!fmtp.parse(line)) {
        return false;
    }
    fmtps_.insert(std::pair(fmtp.get_pt(), fmtp));
    return true;
}

std::string Payload::to_string() const {
    std::ostringstream oss;
    oss << "a=rtpmap:" << std::to_string(payload_type_) << " " << encoding_name_ << "/" << clock_rate_;
    if (encoding_params_.size() > 0) {
        oss << "/" << boost::join(encoding_params_, "/");
    }
    oss << std::endl;

    for (auto & p : rtcp_fbs_) {
        oss << p.to_string();
    }

    for (auto & p : fmtps_) {
        oss << p.second.to_string();
    }
    return oss.str();
}