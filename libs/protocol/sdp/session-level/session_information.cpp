#include <sstream>
#include "session_information.hpp"
#include "base/utils/utils.h"

using namespace mms;
std::string SessionInformation::prefix = "i=";
bool SessionInformation::parse(const std::string & line) {
    std::string::size_type end_pos = line.rfind("\r");
    if (end_pos == std::string::npos) {
        end_pos = line.size() - 1;
    }
    session_information = line.substr(prefix.size(), end_pos - prefix.size());
    return true;
}

std::string SessionInformation::to_string() const {
    std::ostringstream oss;
    oss << prefix << session_information << std::endl;
    return oss.str();
}