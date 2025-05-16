/*
 * @Author: jbl19860422
 * @Date: 2023-08-19 13:59:58
 * @LastEditTime: 2023-08-19 19:40:17
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\config\placeholder\app_placeholder.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include "app_placeholder.h"
#include "core/stream_session.hpp"

using namespace mms;
AppPlaceHolder::AppPlaceHolder() {
    holder_ = "app";
}

AppPlaceHolder::~AppPlaceHolder() {

}

std::string AppPlaceHolder::get_val(StreamSession & session) {
    return session.get_app_name();
}