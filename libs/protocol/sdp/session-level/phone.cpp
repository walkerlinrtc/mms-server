#include <sstream>
#include "phone.hpp"
#include "base/utils/utils.h"
using namespace mms;
std::string Phone::prefix = "p=";
bool Phone::parse(const std::string & line) {
    std::string::size_type end_pos = line.rfind("\r");
    if (end_pos == std::string::npos) {
        end_pos = line.size() - 1;
    }
    phone = line.substr(prefix.size(), end_pos - prefix.size());
    return true;
}

std::string Phone::to_string() const {
    std::ostringstream oss;
    oss << prefix << phone << std::endl;
    return oss.str();
}