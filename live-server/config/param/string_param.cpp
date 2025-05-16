/*
 * @Author: jbl19860422
 * @Date: 2023-08-13 15:41:55
 * @LastEditTime: 2023-08-22 06:38:14
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\config\param\md5_param.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include "string_param.h"
#include "base/utils/utils.h"
using namespace mms;

StringParam::StringParam() : Param("string") {

}

StringParam::~StringParam() {
    
}

int32_t StringParam::param_count() {
    return 1;
}

std::string StringParam::get_val(StreamSession & session, std::vector<std::string> & method_params) {
    ((void)session);
    std::string res;
    for (auto & p : method_params) {
        res += p;
    }
    return res;
}