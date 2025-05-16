/*
 * @Author: jbl19860422
 * @Date: 2023-08-13 15:41:55
 * @LastEditTime: 2023-08-22 06:38:14
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\config\param\md5_param.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include <boost/algorithm/string.hpp>

#include "md5_param.h"
#include "base/utils/utils.h"

using namespace mms;

Md5Param::Md5Param(bool upper) : Param("md5"), upper_(upper) {

}

Md5Param::~Md5Param() {
    
}

int32_t Md5Param::param_count() {
    return 1;
}

std::string Md5Param::get_val(StreamSession & session, std::vector<std::string> & method_params) {
    ((void)session);
    if (method_params.size() < 1) {
        return "";
    }
    std::string res = Utils::md5(method_params[0]);
    if (upper_) {
        boost::algorithm::to_upper(res);
        return res;
    } else {
        boost::algorithm::to_lower(res);
        return res;
    }
}