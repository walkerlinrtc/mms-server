#include <sstream>
#include "email.hpp"
#include "base/utils/utils.h"
using namespace mms;
std::string EmailAddress::prefix = "e=";
bool EmailAddress::parse(const std::string & line) {
    std::string::size_type end_pos = line.rfind("\r");
    if (end_pos == std::string::npos) {
        end_pos = line.size() - 1;
    }
    address = line.substr(prefix.size(), end_pos - prefix.size());
    return true;
}

std::string EmailAddress::to_string() const {
    std::ostringstream oss;
    oss << prefix << address << std::endl;
    return oss.str();
}