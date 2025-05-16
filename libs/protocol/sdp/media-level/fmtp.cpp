/*
 * @Author: jbl19860422
 * @Date: 2022-12-16 22:53:04
 * @LastEditTime: 2023-09-16 12:06:29
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\protocol\sdp\media-level\fmtp.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include <sstream>
#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>
#include "base/utils/utils.h"

#include "fmtp.h"
using namespace mms;
std::string Fmtp::prefix = "a=fmtp:";
std::string Fmtp::empty_str = "";

bool Fmtp::is_my_prefix(const std::string & line)
{
    if (boost::starts_with(line, prefix)) {
        return true;
    } 
    return false;
}

bool Fmtp::parse(const std::string & line)
{
    std::string::size_type end_pos = line.rfind("\r");
    if (end_pos == std::string::npos) {
        end_pos = line.size() - 1;
    }
    std::string valid_string = line.substr(prefix.size(), end_pos - prefix.size());
    std::vector<std::string> vs;
    std::string::size_type space_pos;
    space_pos = valid_string.find(" ");
    if (space_pos == std::string::npos) {
        return false;
    }
    
    std::string pt_str = valid_string.substr(0, space_pos);
    pt_ = std::atoi(pt_str.data());

    std::string param_str = valid_string.substr(space_pos + 1);

    std::vector<std::string> vs_params;
    boost::split(vs_params, param_str, boost::is_any_of(";"));

    for (size_t i = 0; i < vs_params.size(); i++) {
        std::string str = vs_params[i];
        boost::algorithm::trim(str);
        auto pos = str.find("=");
        if (pos == std::string::npos) {
            return false;
        }

        std::string k = str.substr(0, pos);
        std::string v = str.substr(pos + 1);

        fmt_params_[k] = v;
    }

    return true;
}

std::string Fmtp::to_string() const
{
    std::ostringstream oss;
    std::vector<std::string> params;
    for (auto & kv : fmt_params_) {
        params.push_back(kv.first + "=" + kv.second);
    }

    oss << prefix << pt_ << " " << boost::join(params, "; ") << std::endl;
    return oss.str();
}

void Fmtp::add_param(const std::string &k, const std::string & v) {
    fmt_params_[k] = v;
}