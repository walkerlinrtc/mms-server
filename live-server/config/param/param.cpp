/*
 * @Author: jbl19860422
 * @Date: 2023-08-05 15:01:44
 * @LastEditTime: 2023-08-19 18:04:58
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\config\param\param.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include "param.h"

#include "../placeholder/placeholder.h"
#include "server/session.hpp"
#include "md5_param.h"
#include "gettime_param.h"
#include "string_param.h"
#include "hmac_sha1_param.h"
#include "base64_param.h"
#include "add_param.h"
#include "sub_param.h"
#include "bin_to_hex_param.h"

using namespace mms;
Param::Param(const std::string & method_name) : method_name_(method_name) {

}

Param::~Param() {

}

std::shared_ptr<Param> Param::gen_param(const std::string & method_name) {
    if (method_name == "md5_upper") {
        return std::make_shared<Md5Param>(true);
    } else if (method_name == "md5_lower") {
        return std::make_shared<Md5Param>(false);
    } else if (method_name == "get_time") {
        return std::make_shared<GetTimeParam>();
    } else if (method_name == "string") {
        return std::make_shared<StringParam>();
    } else if (method_name == "hmac_sha1") {
        return std::make_shared<HMACSha1Param>();
    } else if (method_name == "base64") {
        return std::make_shared<Base64Param>();
    } else if (method_name == "add") {
        return std::make_shared<AddParam>();
    } else if (method_name == "sub") {
        return std::make_shared<SubParam>();
    } else if (method_name == "bin_to_hex") {
        return std::make_shared<BinToHexParam>();
    }
    return nullptr;
}

int32_t Param::parse(const std::string & str) {
    ((void)str);
    return 0;
}

void Param::add_placeholder(const std::vector<std::shared_ptr<PlaceHolder>> & method_param_holder) {
    method_param_holders_.push_back(method_param_holder);
}

std::string Param::get_val(StreamSession & session) {
    std::vector<std::string> method_params;
    for (auto & mph : method_param_holders_) {
        std::string p;
        for (auto & h : mph) {
            p += h->get_val(session);
        }
        method_params.emplace_back(p);
    }

    return get_val(session, method_params);
}
