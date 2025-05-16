#include <sstream>
#include "sendrecv.hpp"
using namespace mms;
std::string SendRecvAttr::prefix = "a=sendrecv";

bool SendRecvAttr::parse(const std::string &line)
{
    std::string::size_type end_pos = line.rfind("\r");
    if (end_pos == std::string::npos)
    {
        end_pos = line.size() - 1;
    }

    return true;
}

std::string SendRecvAttr::to_string() const
{
    std::ostringstream oss;
    oss << prefix << std::endl;
    return oss.str();
}