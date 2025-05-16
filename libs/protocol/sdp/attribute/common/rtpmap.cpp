/*
 * @Author: jbl19860422
 * @Date: 2023-09-16 11:45:26
 * @LastEditTime: 2023-09-16 12:08:09
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\protocol\sdp\attribute\common\rtpmap.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include <sstream>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>
#include "rtpmap.h"
#include "base/utils/utils.h"
using namespace mms;
//a=rtpmap:<payload type> <encoding name>/<clock rate> [/<encoding
//  parameters>]
std::string Rtpmap::prefix = "a=rtpmap:";
bool Rtpmap::parse(const std::string & line) {
    std::string::size_type end_pos = line.rfind("\r");
    if (end_pos == std::string::npos) {
        end_pos = line.size() - 1;
    }
    std::string valid_string = line.substr(prefix.size(), end_pos - prefix.size());

    std::vector<std::string> vs;
    boost::split(vs, valid_string, boost::is_any_of(" "));
    if (vs.size() != 2) {
        return false;
    }

    payload_type = std::atoi(vs[0].c_str());
    std::vector<std::string> tmp;
    boost::split(tmp, vs[1], boost::is_any_of("/"));
    if (tmp.size() < 2) {
        return false;
    }
    encoding_name = tmp[0];
    clock_rate = std::atoi(tmp[1].c_str());
    for (size_t i = 2; i < tmp.size(); i++) {
        encoding_params.emplace_back(tmp[i]);
    }
    return true;
}

std::string Rtpmap::to_string() const {
    std::ostringstream oss;
    oss << prefix << std::to_string(payload_type) << " " << encoding_name << "/" << clock_rate;
    if (encoding_params.size() > 0) {
        oss << boost::join(encoding_params, "/");
    }
    oss << std::endl;
    return oss.str();
}