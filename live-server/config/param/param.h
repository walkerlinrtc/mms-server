/*
 * @Author: jbl19860422
 * @Date: 2023-08-03 22:13:23
 * @LastEditTime: 2023-08-19 17:57:50
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\config\param\param.h
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include <string>
#include <vector>
#include <memory>

namespace mms {
class PlaceHolder;
class StreamSession;
class OriginPullConfig;
class Param {
public:
    Param(const std::string & method_name);
    virtual ~Param();
    static std::shared_ptr<Param> gen_param(const std::string & method_name);
    virtual int32_t param_count() = 0;
    int32_t parse(const std::string & str);
    void add_placeholder(const std::vector<std::shared_ptr<PlaceHolder>> & method_param_holder);
    std::string get_val(StreamSession & session);
protected:
    virtual std::string get_val(StreamSession & session, std::vector<std::string> & method_params) = 0;
private:
    std::string method_name_;//获取方法
    int32_t level_ = 0;
    int32_t index_ = 0;
    std::vector<std::vector<std::shared_ptr<PlaceHolder>>> method_param_holders_;
};
};
