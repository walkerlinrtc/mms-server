#include <sstream>
#include "tool.hpp"
#include "base/utils/utils.h"
using namespace mms;
std::string ToolAttr::prefix = "a=tool:";

bool ToolAttr::parse(const std::string & line) {
    std::string::size_type end_pos = line.rfind("\r");
    if (end_pos == std::string::npos) {
        end_pos = line.size() - 1;
    }
    name_ = line.substr(prefix.size(), end_pos - prefix.size());
    return true;
}

std::string ToolAttr::to_string() const {
    std::ostringstream oss;
    oss << prefix << name_ << std::endl;
    return oss.str();
}