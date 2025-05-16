/*
 * @Author: jbl19860422
 * @Date: 2023-07-09 18:51:22
 * @LastEditTime: 2023-07-09 18:51:39
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\config\http_callback_config.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include <boost/algorithm/string.hpp>
#include <string_view>
#include <boost/utility/string_view.hpp>

#include "http_callback_config.h"
#include "base/utils/utils.h"
#include "placeholder/domain_placeholder.h"
#include "placeholder/app_placeholder.h"
#include "placeholder/stream_name_placeholder.h"
#include "placeholder/stream_type_placeholder.h"
#include "placeholder/string_placeholder.h"
#include "placeholder/param_placeholder.h"
#include "placeholder/url_param_placeholder.h"

#include "param/param.h"
#include "param/md5_param.h"

#include "service/dns/dns_service.hpp"
#include "core/stream_session.hpp"
#include "spdlog/spdlog.h"

#include "placeholder_generator.h"

using namespace mms;

int32_t HttpCallbackConfig::load_config(const YAML::Node & node) {
    // auto params_node = node["params"];
    // if (params_node.IsDefined()) {
    //     if (params_node.size() > 0) {
    //         for(YAML::const_iterator it = params_node.begin(); it != params_node.end(); it++) {
    //             std::string name = it->first.as<std::string>();
    //             std::string param_pattern = it->second.as<std::string>();
    //             auto left_bracket_pos = param_pattern.find("(");
    //             if (left_bracket_pos == std::string::npos) {//解析错误
    //                 return -2;
    //             }
                
    //             if (param_pattern[param_pattern.size() - 1] != ')') {
    //                 return -3;
    //             }

    //             std::string method_name = param_pattern.substr(0, left_bracket_pos);
    //             std::shared_ptr<Param> param = Param::gen_param(method_name);
    //             if (!param) {
    //                 return -4;
    //             }
    //             std::string method_params_list = param_pattern.substr(left_bracket_pos + 1, param_pattern.size() - left_bracket_pos - 2);
    //             std::vector<std::string> method_params;
    //             boost::split(method_params, method_params_list, boost::is_any_of(","));
    //             for (size_t i = 0; i < method_params.size(); i++) {
    //                 if (0 != parse_method_param(node, param, method_params[i])) {
    //                     return -5;
    //                 }
    //             }
    //             params_.insert(std::pair(name, param));
    //         }
    //     }
    // }

    auto url_node = node["url"];
    if (!url_node.IsDefined()) {
        return -6;
    }

    unformat_url_ = url_node.as<std::string>();
    if (0 != PlaceHolderGenerator::parse_string_to_placeholders(node, unformat_url_, url_holders_)) {
        return -7;
    }

    if (url_holders_.size() < 1) {
        return -8;
    }

    auto holder0 = url_holders_[0]->get_holder();
    std::vector<std::string> vs;
    boost::split(vs, holder0, boost::is_any_of("/"));//http://domain/, rtmp://domain/
    if (vs.size() < 3) {
        return -9;
    }
    auto p = vs[0];
    if (p.starts_with("https")) {
        protocol_ = "https";
    } else if (p.starts_with("http")) {
        protocol_ = "http";
    } else {
        return -10;
    }

    auto colon_pos = vs[2].find_first_of(":");
    if (colon_pos == std::string::npos) {
        target_domain_ = vs[2];
        if (protocol_ == "https") {
            port_ = 443;
        } else {
            port_ = 80;
        }
    } else {
        target_domain_ = vs[2].substr(0, colon_pos);
        try {
            port_ = std::atoi(vs[2].substr(colon_pos+1).c_str());
        } catch (std::exception & exp) {
            return -11;
        }
    }

    auto body_node = node["body"];
    if (body_node.IsDefined() && body_node.IsScalar()) {
        unformat_body_ = body_node.as<std::string>();
        auto ret = PlaceHolderGenerator::parse_string_to_placeholders(node, unformat_body_, body_holders_);
        if (0 != ret) {
            spdlog::error("parse_string_to_placeholders failed, code:{}", ret);
            return -13;
        }
    }

    auto headers_node = node["headers"];
    if (headers_node.IsDefined() && headers_node.IsMap()) {
        for (auto it = headers_node.begin(); it != headers_node.end(); it++) {
            auto key = it->first.as<std::string>();
            auto value = it->second.as<std::string>();
            if (0 != PlaceHolderGenerator::parse_string_to_placeholders(node, value, headers_holders_[key])) {
                return -14;
            }
        }
    }

    DnsService::get_instance().add_domain(target_domain_);
    return 0;
}

std::string HttpCallbackConfig::gen_url(std::shared_ptr<StreamSession> s) const {
    std::string url;
    spdlog::info("url holders count:{}", url_holders_.size());
    for (auto & h : url_holders_) {
        url += h->get_val(*s);
    }
    return url;
}

std::string HttpCallbackConfig::gen_body(std::shared_ptr<StreamSession> s) const {
    std::string url;
    for (auto & h : body_holders_) {
        url += h->get_val(*s);
    }
    return url;
}

std::unordered_map<std::string, std::string> HttpCallbackConfig::gen_headers(std::shared_ptr<StreamSession> s) const {
    std::unordered_map<std::string, std::string> headers;
    for (auto it = headers_holders_.begin(); it != headers_holders_.end(); it++) {
        std::string value;
        for (auto & h : it->second) {
            value += h->get_val(*s);
        }
        headers[it->first] = value;
    }
    return headers;
}