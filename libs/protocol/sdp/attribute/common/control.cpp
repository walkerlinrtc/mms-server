#include <sstream>
#include "control.hpp"
using namespace mms;

std::string Control::prefix = "a=control:";
bool Control::parse(const std::string &line)
{
    std::string::size_type end_pos = line.rfind("\r");
    if (end_pos == std::string::npos)
    {
        end_pos = line.size() - 1;
    }
    val_ = line.substr(prefix.size(), end_pos - prefix.size());
    return true;
}

std::string Control::to_string() const
{
    std::ostringstream oss;
    oss << prefix << val_ << std::endl;
    return oss.str();
}