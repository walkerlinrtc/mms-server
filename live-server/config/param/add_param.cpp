/*
 * @Author: jbl19860422
 * @Date: 2023-08-13 15:41:55
 * @LastEditTime: 2023-08-22 06:38:14
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\config\param\md5_param.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include "add_param.h"
#include "base/utils/utils.h"
using namespace mms;

AddParam::AddParam() : Param("add") {

}

AddParam::~AddParam() {
    
}

int32_t AddParam::param_count() {
    return 2;
}

std::string AddParam::get_val(StreamSession & session, std::vector<std::string> & method_params) {
    ((void)session);
    if (method_params.size() < 2) {
        return "";
    }
    
    try {
        auto a = std::atoi(method_params[0].c_str());
        auto b = std::atoi(method_params[1].c_str());
        return std::to_string(a+b);
    } catch (std::exception& exp) {
        return "";
    }
    return "";
}