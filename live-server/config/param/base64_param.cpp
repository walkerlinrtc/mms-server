/*
 * @Author: jbl19860422
 * @Date: 2023-08-13 15:41:55
 * @LastEditTime: 2023-08-22 06:38:14
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\config\param\md5_param.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include "base64_param.h"
#include "base/utils/utils.h"
#include "log/log.h"

using namespace mms;

Base64Param::Base64Param() : Param("base64") {

}

Base64Param::~Base64Param() {
    
}

int32_t Base64Param::param_count() {
    return 1;
}

std::string Base64Param::get_val(StreamSession & session, std::vector<std::string> & method_params) {
    ((void)session);
    if (method_params.size() < 1) {
        return "";
    }
    std::string res;
    Utils::encode_base64(method_params[0], res);
    return res;
}