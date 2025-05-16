#include <sstream>
#include "maxptime.hpp"
using namespace mms;

std::string MaxPTimeAttr::prefix = "a=maxptime:";
bool MaxPTimeAttr::parse(const std::string &line)
{
    std::string::size_type end_pos = line.rfind("\r");
    if (end_pos == std::string::npos)
    {
        end_pos = line.size() - 1;
    }
    std::string stime = line.substr(prefix.size(), end_pos - prefix.size());
    max_packet_time = std::atoi(stime.c_str());
    return true;
}

std::string MaxPTimeAttr::to_string() const
{
    std::ostringstream oss;
    oss << prefix << max_packet_time << std::endl;
    return oss.str();
}