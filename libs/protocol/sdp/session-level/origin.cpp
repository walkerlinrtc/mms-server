#include <exception>
#include <sstream>
#include <boost/algorithm/string.hpp>

#include "base/utils/utils.h"
#include "origin.hpp"

using namespace mms;
std::string Origin::prefix = "o=";
bool Origin::parse(const std::string & line) {
    std::string::size_type end_pos = line.rfind("\r");
    if (end_pos == std::string::npos) {
        end_pos = line.size() - 1;
    }
    std::string valid_string = line.substr(prefix.size(), end_pos - prefix.size());

    std::vector<std::string> vs;
    boost::split(vs, valid_string, boost::is_any_of(" "));
    if (vs.size() != 6) {
        return false;
    }

    try {
        username_ = vs[0];
        session_id_ = std::atoll(vs[1].c_str());
        session_version_ = std::atoi(vs[2].c_str());
        nettype_ = vs[3];
        addrtype_ = vs[4];
        unicast_address_ = vs[5];
    } catch(std::exception & e) {
        return false;
    }
    return true;
}

std::string Origin::to_string() const {
    std::string line;
    std::ostringstream oss;
    oss << prefix << username_ << " " << session_id_ << " " << session_version_ << " " << nettype_ << " " << addrtype_ << " " << unicast_address_ << std::endl;
    return oss.str();
}