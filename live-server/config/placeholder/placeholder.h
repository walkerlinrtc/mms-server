/*
 * @Author: jbl19860422
 * @Date: 2023-07-31 20:54:31
 * @LastEditTime: 2023-07-31 21:26:12
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\config\placeholder.h
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include <string>
namespace mms {
class StreamSession;
class OriginPullConfig;
class PlaceHolder {
public:
    PlaceHolder();
    virtual ~PlaceHolder();
    virtual std::string get_val(StreamSession & session);//这里session肯定存在，所以直接用引用，不用智能指针
    const std::string & get_holder() const {
        return holder_;
    }
protected:
    std::string holder_;//参数名称
};
};