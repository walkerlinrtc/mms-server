#include <sstream>
#include <boost/algorithm/string.hpp>

#include "base/utils/utils.h"
#include "rtcp.h"

using namespace mms;
std::string RtcpAttr::prefix = "a=rtcp:";
bool RtcpAttr::parse(const std::string & line) {
    if (!boost::starts_with(line, prefix)) {
        return false;
    }
    
    std::string::size_type end_pos = line.rfind("\r");
    if (end_pos == std::string::npos) {
        end_pos = line.size() - 1;
    }
    std::string valid_string = line.substr(prefix.size(), end_pos - prefix.size());

    std::vector<std::string> vs;
    boost::split(vs, valid_string, boost::is_any_of(" "));
    if (vs.size() < 1) {
        return false;
    }

    port = std::atoi(vs[0].c_str());
    if (vs.size() >= 2) {
        nettype = vs[1];
    }

    if (vs.size() >= 3) {
        addrtype = vs[2];
    }
    
    if (vs.size() >= 4) {
        connection_address = vs[3];
    }
    return true;
}

std::string RtcpAttr::to_string() const {
    std::ostringstream oss;
    oss << prefix << nettype << " " << addrtype << " " << connection_address;
    oss << std::endl;
    return oss.str();
}