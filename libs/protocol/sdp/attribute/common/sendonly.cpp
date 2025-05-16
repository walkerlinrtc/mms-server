#include <sstream>
#include "sendonly.hpp"
using namespace mms;
std::string SendOnlyAttr::prefix = "a=sendonly";

bool SendOnlyAttr::parse(const std::string &line)
{
    std::string::size_type end_pos = line.rfind("\r");
    if (end_pos == std::string::npos)
    {
        end_pos = line.size() - 1;
    }

    return true;
}

std::string SendOnlyAttr::to_string() const
{
    std::ostringstream oss;
    oss << prefix << std::endl;
    return oss.str();
}