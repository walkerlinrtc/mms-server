
#include <sstream>
#include <iostream>
#include "protocol_version.hpp"
#include "base/utils/utils.h"
using namespace mms;

std::string ProtocolVersion::prefix = "v=";

bool ProtocolVersion::parse(const std::string & line) {
    std::string::size_type end_pos = line.rfind("\r");
    if (end_pos == std::string::npos) {
        end_pos = line.size() - 1;
    }
    std::string sversion = line.substr(prefix.size(), end_pos - prefix.size());
    try {
        version = std::stoi(sversion);
    } catch (std::exception & e) {
        return false;
    }
    
    return true;
}

void ProtocolVersion::set_version(int v) {
    version = v;
}

int ProtocolVersion::get_version() {
    return version;
}

std::string ProtocolVersion::to_string() const {
    std::ostringstream oss;
    oss << prefix << std::to_string(version) << std::endl;
    return oss.str();
}