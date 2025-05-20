/*
 * @Author: jbl19860422
 * @Date: 2023-08-13 15:41:55
 * @LastEditTime: 2023-08-22 06:38:14
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\config\param\md5_param.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include "hmac_sha1_param.h"
#include "base/utils/utils.h"
#include "log/log.h"
using namespace mms;

HMACSha1Param::HMACSha1Param() : Param("hmac_sha1") {

}

HMACSha1Param::~HMACSha1Param() {
    
}

int32_t HMACSha1Param::param_count() {
    return 2;
}

std::string HMACSha1Param::get_val(StreamSession & session, std::vector<std::string> & method_params) {
    ((void)session);
    if (method_params.size() < 2) {
        return "";
    }
    
    std::string res = Utils::calc_hmac_sha1(method_params[0], method_params[1]);
    std::string hex_str;
    Utils::bin_to_hex_str(res, hex_str);
    return hex_str;
}