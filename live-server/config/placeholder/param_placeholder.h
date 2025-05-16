/*
 * @Author: jbl19860422
 * @Date: 2023-08-05 15:01:53
 * @LastEditTime: 2023-08-11 15:16:47
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\config\placeholder\param_placeholder.h
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include <memory>
#include "placeholder.h"
namespace mms {
class Param;
class StreamSession;

class ParamPlaceHolder : public PlaceHolder {
public:
    ParamPlaceHolder(const std::string & param_name, std::shared_ptr<Param> param);
    virtual ~ParamPlaceHolder();
public:
    std::string get_val(StreamSession & session) final;
protected:
    std::string param_name_;
    std::shared_ptr<Param> param_;
};
};