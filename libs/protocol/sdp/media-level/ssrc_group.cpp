/*
 * @Author: jbl19860422
 * @Date: 2023-09-16 10:32:44
 * @LastEditTime: 2023-09-16 12:04:40
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\protocol\sdp\media-level\ssrc_group.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include <vector>
#include <sstream>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string.hpp>

#include "spdlog/spdlog.h"
#include "base/utils/utils.h"
#include "ssrc_group.h"
using namespace mms;

std::string SsrcGroup::prefix = "a=ssrc-group:";
bool SsrcGroup::parse(const std::string &line)
{
    std::string::size_type end_pos = line.rfind("\r");
    if (end_pos == std::string::npos)
    {
        end_pos = line.size() - 1;
    }
    std::string valid_string = line.substr(prefix.size(), end_pos - prefix.size());

    std::vector<std::string> vs;
    boost::split(vs, valid_string, boost::is_any_of(" "));
    if (vs.size() < 2)
    {
        return false;
    }
    semantics = vs[0];
    try
    {
        for (size_t i = 1; i < vs.size(); i++) {
            uint32_t ssrc_id = std::atoi(vs[i].c_str());
            ssrcs_[ssrc_id];
        }
    }
    catch (std::exception &e)
    {
        return false;
    }
    return true;
}

bool SsrcGroup::parse_ssrc(const std::string & line) {
    std::string::size_type end_pos = line.rfind("\r");
    const static std::string pre = "a=ssrc:";
    std::string valid_string = line.substr(pre.size(), end_pos - prefix.size());
    std::vector<std::string> vs;
    boost::split(vs, valid_string, boost::is_any_of(" "));
    if (vs.size() < 2) {
        return false;
    }

    uint32_t id = std::atoi(vs[0].c_str());
    if (!ssrcs_[id].parse(line)) {
        return false;
    }
    return true;
}

std::string SsrcGroup::to_string() const {
    std::ostringstream oss;
    oss << prefix << semantics << " ";
    std::vector<std::string> v;
    for (auto & p : ssrcs_) {
        v.push_back(std::to_string(p.first));
    }
    oss << boost::join(v, " ") << std::endl;
    return oss.str();
}