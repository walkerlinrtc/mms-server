#include <sstream>
#include <boost/algorithm/string.hpp>

#include "ssrc.h"
#include "base/utils/utils.h"
using namespace mms;
std::string Ssrc::prefix = "a=ssrc:";
bool Ssrc::parse(const std::string & line) {
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

    id_ = std::atoi(vs[0].c_str());
    if (boost::starts_with(vs[1], "cname:")) {
        cname_ = vs[1].substr(6);
    } else if (boost::starts_with(vs[1], "mslabel:")) {
        mslabel_ = vs[1].substr(8);
    } else if (boost::starts_with(vs[1], "label:")) {
        label_ = vs[1].substr(6);
    } else if (boost::starts_with(vs[1], "msid:")) {
        if (vs.size() < 3) {
            return false;
        }

        mslabel_ = vs[1].substr(5);
        label_ = vs[2];
    }

    return true;
}

uint32_t Ssrc::parse_id_only(const std::string & line) {
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

    uint32_t id = std::atoi(vs[0].c_str());
    return id;
}

std::string Ssrc::to_string() const {
    std::ostringstream oss;
    oss << prefix << id_ << " cname:" << cname_ << std::endl;
    oss << prefix << id_ << " msid:" << mslabel_ << " " << label_ << std::endl;
    oss << prefix << id_ << " mslabel:" << mslabel_ << std::endl;
    oss << prefix << id_ << " label:" << label_ << std::endl;
    return oss.str();
}