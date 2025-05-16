#include "spdlog/spdlog.h"
#include <vector>
#include <string_view>
#include <sstream>
#include <boost/algorithm/string.hpp>

#include "bandwidth.hpp"
using namespace mms;
std::string BandWidth::prefix = "b=";

bool BandWidth::parse(const std::string & line) {
    std::string::size_type end_pos = line.rfind("\r");
    if (end_pos == std::string::npos) {
        end_pos = line.size() - 1;
    }
    std::string_view l = line.substr(prefix.size(), end_pos - prefix.size());
    std::string::size_type comma_pos = l.find_first_of(":");
    if (comma_pos == std::string_view::npos) {
        return false;
    }

    bw_type = l.substr(0, comma_pos);
    try {
        bandwidth = std::atoi(l.substr(comma_pos + 1).data());
    } catch (std::exception & e) {
        return false;
    }
    return true;
}

std::string BandWidth::to_string() const {
    std::ostringstream oss;
    oss << prefix << bw_type << ":" << std::to_string(bandwidth) << std::endl;
    return oss.str();
}