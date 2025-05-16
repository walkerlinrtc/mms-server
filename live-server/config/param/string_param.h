/*
 * @Author: jbl19860422
 * @Date: 2023-08-13 15:27:25
 * @LastEditTime: 2023-08-13 15:41:34
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\config\md5_param.h
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include <vector>
#include <string>

#include "param.h"
namespace mms {
class StringParam : public Param {
public:
    StringParam();
    virtual ~StringParam();
    int32_t param_count();
public:
    std::string get_val(StreamSession & session, std::vector<std::string> & method_params) final;
};
};