/*
 * @Author: jbl19860422
 * @Date: 2023-08-13 15:41:55
 * @LastEditTime: 2023-08-22 06:38:14
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\config\param\md5_param.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include "bin_to_hex_param.h"
#include "base/utils/utils.h"
#include "log/log.h"

using namespace mms;

BinToHexParam::BinToHexParam() : Param("bin_to_hex") {

}

BinToHexParam::~BinToHexParam() {
    
}

int32_t BinToHexParam::param_count() {
    return 1;
}

std::string BinToHexParam::get_val(StreamSession & session, std::vector<std::string> & method_params) {
    ((void)session);
    if (method_params.size() < 1) {
        return "";
    }
    std::string res;
    Utils::bin_to_hex_str(method_params[0], res);
    return res;
}