/*
 * @Author: jbl19860422
 * @Date: 2023-08-13 15:41:55
 * @LastEditTime: 2023-08-19 18:11:04
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\config\param\gettime_param.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include "gettime_param.h"
#include "base/utils/utils.h"
using namespace mms;

GetTimeParam::GetTimeParam() : Param("get_time") {

}

GetTimeParam::~GetTimeParam() {
    
}

int32_t GetTimeParam::param_count() {
    return 0;
}

std::string GetTimeParam::get_val(StreamSession & session, std::vector<std::string> & method_params) {
    ((void)session);
    ((void)method_params);
    return std::to_string(time(NULL));
}