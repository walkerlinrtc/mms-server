/*
 * @Author: jbl19860422
 * @Date: 2023-08-05 15:01:44
 * @LastEditTime: 2023-08-19 17:54:12
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\config\placeholder\param_placeholder.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include "param_placeholder.h"
#include "server/session.hpp"
#include "../param/param.h"

using namespace mms;
ParamPlaceHolder::ParamPlaceHolder(const std::string & param_name, std::shared_ptr<Param> param) : param_name_(param_name), param_(param) {
    holder_ = param_name;
}

ParamPlaceHolder::~ParamPlaceHolder() {

}

std::string ParamPlaceHolder::get_val(StreamSession & session) {
    return param_->get_val(session);
}