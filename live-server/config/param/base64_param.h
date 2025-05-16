/*
 * @Author: jbl19860422
 * @Date: 2023-08-13 15:27:25
 * @LastEditTime: 2023-08-13 15:41:34
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\config\base64_param.h
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include <string>
#include <vector>

#include "param.h"
namespace mms {
class Base64Param : public Param {
public:
    Base64Param();
    virtual ~Base64Param();
    int32_t param_count();
public:
    std::string get_val(StreamSession & session, std::vector<std::string> & method_params) final;
};
};