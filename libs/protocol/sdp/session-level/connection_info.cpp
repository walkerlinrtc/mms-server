#include <sstream>
#include <boost/algorithm/string.hpp>

#include "base/utils/utils.h"
#include "connection_info.hpp"

using namespace mms;
std::string ConnectionInfo::prefix = "c=";
bool ConnectionInfo::parse(const std::string & line) {
    std::string::size_type end_pos = line.rfind("\r");
    if (end_pos == std::string::npos) {
        end_pos = line.size() - 1;
    }
    std::string valid_string = line.substr(prefix.size(), end_pos - prefix.size());

    std::vector<std::string> vs;
    boost::split(vs, valid_string, boost::is_any_of(" "));
    if (vs.size() != 3) {
        return false;
    }

    nettype_ = vs[0];
    addrtype_ = vs[1];
    std::string conn_addr_info = vs[2];
    
    boost::split(vs, conn_addr_info, boost::is_any_of("/"));
    connection_address_ = vs[0];
    if (vs.size() >= 2) {
        ttl = std::atoi(vs[1].c_str());
    }

    if (vs.size() >= 3) {
        num_of_addr = std::atoi(vs[2].c_str());
    }
    
    return true;
}

std::string ConnectionInfo::to_string() const {
    std::ostringstream oss;
    oss << prefix << nettype_ << " " << addrtype_ << " " << connection_address_;
    if (num_of_addr > 1) {
        oss << "/" << ttl << "/" << num_of_addr;
    }
    oss << std::endl;
    return oss.str();
}