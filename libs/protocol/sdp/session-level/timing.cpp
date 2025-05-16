#include <sstream>
#include <vector>
#include <string>
#include <sstream>
#include <boost/algorithm/string.hpp>
#include "base/utils/utils.h"
#include "timing.hpp"

using namespace mms;
std::string Timing::prefix = "t=";
bool Timing::parse(const std::string & line) {
    std::string::size_type end_pos = line.rfind("\r");
    if (end_pos == std::string::npos) {
        end_pos = line.size() - 1;
    }
    std::string valid_string = line.substr(prefix.size(), end_pos - prefix.size());

    std::vector<std::string> vs;
    boost::split(vs, valid_string, boost::is_any_of(" "));
    if (vs.size() != 2) {
        return false;
    }

    try {
        start_time_ = std::atoll(vs[0].c_str());
        stop_time_ = std::atoll(vs[1].c_str());
    } catch(std::exception & e) {
        return false;
    }
    return true;
}

std::string Timing::to_string() const {
    std::ostringstream oss;
    oss << prefix << start_time_ << " " << stop_time_ << std::endl;
    return oss.str();
}