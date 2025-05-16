#include <sstream>
#include <vector>
#include <string>
#include <sstream>
#include <boost/algorithm/string.hpp>

#include "base/utils/utils.h"

#include "candidate.h"

using namespace mms;
std::string Candidate::prefix = "a=candidate:";
bool Candidate::parse(const std::string &line)
{
    std::string::size_type end_pos = line.rfind("\r");
    if (end_pos == std::string::npos)
    {
        end_pos = line.size() - 1;
    }
    std::string valid_string = line.substr(prefix.size(), end_pos - prefix.size());
    std::vector<std::string> vs;
    boost::split(vs, valid_string, boost::is_any_of(" "));
    if (vs.size() < 8)
    {
        return false;
    }
    foundation_ = vs[0];
    component_id_ = std::atoi(vs[1].c_str());
    transport_ = vs[2];
    priority_ = std::atoi(vs[3].c_str());
    address_ = vs[4];
    port_ = std::atoi(vs[5].c_str());
    std::string t = vs[6]; // typ
    if (vs[7] == "host")
    {
        cand_type_ = CAND_TYPE_HOST;
    }
    else if (vs[7] == "srflx")
    {
        cand_type_ = CAND_TYPE_SRFLX;
    }
    else if (vs[7] == "prflx")
    {
        cand_type_ = CAND_TYPE_PRFLX;
    }
    else if (vs[7] == "relay")
    {
        cand_type_ = CAND_TYPE_RELAY;
    }

    return true;
}

std::string Candidate::to_string() const
{
    std::ostringstream oss;
    std::string cand_type;
    if (cand_type_ == CAND_TYPE_HOST)
    {
        cand_type = "host";
    }
    else if (cand_type_ == CAND_TYPE_SRFLX)
    {
        cand_type = "srflx";
    }
    else if (cand_type_ == CAND_TYPE_PRFLX)
    {
        cand_type = "prflx";
    }
    else if (cand_type_ == CAND_TYPE_RELAY)
    {
        cand_type = "relay";
    }

    oss << prefix << foundation_ << " " << component_id_ << " " << transport_ << " " << priority_ << " " << address_ << " " << port_ << " typ " << cand_type;
    if (cand_type_ == CAND_TYPE_SRFLX)
    {
        oss << " " << rel_addr_ << " " << rel_port_;
    }

    for (auto &p : exts_)
    {
        oss << " " << p.first << " " << p.second;
    }

    oss << std::endl;
    return oss.str();
}
