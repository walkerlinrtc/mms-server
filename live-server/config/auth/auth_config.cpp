/*
 * @Author: jbl19860422
 * @Date: 2023-08-03 22:13:23
 * @LastEditTime: 2023-08-22 07:35:53
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\config\source_config.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include <boost/algorithm/string.hpp>
#include <string_view>
#include <boost/utility/string_view.hpp>

#include "log/log.h"

#include "auth_config.h"
#include "check_config.h"

#include "../placeholder/domain_placeholder.h"
#include "../placeholder/app_placeholder.h"
#include "../placeholder/stream_name_placeholder.h"
#include "../placeholder/stream_type_placeholder.h"
#include "../placeholder/string_placeholder.h"
#include "../placeholder/param_placeholder.h"
#include "../placeholder/url_param_placeholder.h"

#include "../param/param.h"
#include "../param/md5_param.h"
#include "../param/string_param.h"

using namespace mms;
AuthConfig::AuthConfig() {

}

AuthConfig::~AuthConfig() {

}

int32_t AuthConfig::load(const YAML::Node & node) {
    auto params_node = node["params"];
    if (params_node.IsDefined()) {
        if (params_node.size() > 0) {
            for(YAML::const_iterator it = params_node.begin(); it != params_node.end(); it++) {
                std::string name = it->first.as<std::string>();
                std::string param_pattern = it->second.as<std::string>();
                auto left_bracket_pos = param_pattern.find("(");
                if (left_bracket_pos == std::string::npos) {//解析错误
                    return -2;
                }
                
                if (param_pattern[param_pattern.size() - 1] != ')') {
                    return -3;
                }

                std::string method_name = param_pattern.substr(0, left_bracket_pos);
                std::shared_ptr<Param> param = Param::gen_param(method_name);
                if (!param) {
                    return -4;
                }
                std::string method_params_list = param_pattern.substr(left_bracket_pos + 1, param_pattern.size() - left_bracket_pos - 2);
                std::vector<std::string> method_params;
                boost::split(method_params, method_params_list, boost::is_any_of(","));
                for (size_t i = 0; i < method_params.size(); i++) {
                    if (0 != parse_method_param(node, param, method_params[i])) {
                        return -5;
                    }
                }
                params_.insert(std::pair(name, param));
            }
        }
    }

    auto checks_node = node["checks"];
    if (!checks_node.IsDefined() || !checks_node.IsSequence()) {
        return -6;
    }

    for (size_t i = 0; i < checks_node.size(); i++) {
        YAML::Node check_node = checks_node[i];
        if (0 != parse_check_config(check_node)) {
            return -7;
        }
    }

    return 0;
}

std::shared_ptr<Param> AuthConfig::recursive_search_param_node(const YAML::Node & node, const std::string & param_name) {
    auto params_node = node["params"];
    if (params_node.IsDefined()) {
        if (params_node.size() > 0) {
            for(YAML::const_iterator it = params_node.begin(); it != params_node.end(); it++) {
                std::string name = it->first.as<std::string>();
                if (name != param_name) {
                    continue;
                }

                std::string param_pattern = it->second.as<std::string>();
                auto left_bracket_pos = param_pattern.find("(");
                if (left_bracket_pos == std::string::npos) {//解析错误
                    return nullptr;
                }
                
                if (param_pattern[param_pattern.size() - 1] != ')') {
                    return nullptr;
                }

                std::string method_name = param_pattern.substr(0, left_bracket_pos);
                CORE_INFO("method_name:{}", method_name);
                std::shared_ptr<Param> param = Param::gen_param(method_name);
                if (!param) {
                    return nullptr;
                }
                std::string method_params_list = param_pattern.substr(left_bracket_pos + 1, param_pattern.size() - left_bracket_pos - 2);
                std::vector<std::string> method_params;
                boost::split(method_params, method_params_list, boost::is_any_of(","));
                for (size_t i = 0; i < method_params.size(); i++) {
                    if (0 != parse_method_param(node, param, method_params[i])) {
                        return nullptr;
                    }
                }
                params_.insert(std::pair(name, param));
                return param;
            }
        }
    }
    return nullptr;
}

int32_t AuthConfig::parse_check_config(const YAML::Node & node) {
    std::string check_pattern = node.as<std::string>();
    auto left_bracket_pos = check_pattern.find(" ");
    if (left_bracket_pos == std::string::npos) {//解析错误
        return -1;
    }

    auto right_bracket_pos = check_pattern.find_last_of(" ");
    if (right_bracket_pos == std::string::npos) {
        return -2;
    }

    std::string method_name = check_pattern.substr(left_bracket_pos + 1, right_bracket_pos - left_bracket_pos - 1);
    boost::algorithm::trim(method_name);

    std::shared_ptr<CheckConfig> check = CheckConfig::gen_check(method_name);
    if (!check) {
        return -3;
    }
    std::string param1 = check_pattern.substr(0, left_bracket_pos);
    std::string param2 = check_pattern.substr(right_bracket_pos + 1, check_pattern.size() - right_bracket_pos - 1);
    if (0 != parse_check_param(node, check, param1)) {
        return -4;
    }

    if (0 != parse_check_param(node, check, param2)) {
        return -5;
    }
    
    checks_.push_back(check);
    return 0;
}

int32_t AuthConfig::parse_method_param(const YAML::Node & node, std::shared_ptr<Param> param_holder, const std::string & method_param) {
    size_t i = 0;
    std::vector<std::shared_ptr<PlaceHolder>> method_param_holders;
    std::string seg;
    bool is_string_placeholder = true;
    while (i < method_param.size()) {
        if (i + 1 < method_param.size()) {
            if (method_param[i] == '$' && method_param[i+1] == '{') {
                if (!seg.empty()) {
                    auto h = gen_holder(node, seg, is_string_placeholder);
                    if (h) {
                        method_param_holders.push_back(h);
                    }
                    seg = "";
                }
                i+=2;
                is_string_placeholder = false;
                while (i < method_param.size() && method_param[i] != '}') {
                    seg.append(1, method_param[i]);
                    i++;
                }
                auto h = gen_holder(node, seg, is_string_placeholder);
                if (h) {
                    method_param_holders.push_back(h);
                }
                seg = "";
            } else {
                is_string_placeholder = true;
                seg.append(1, method_param[i]);
            }
        } else {
            is_string_placeholder = true;
            seg.append(1, method_param[i]);
        }

        i++;
    }

    if (!seg.empty()) {
        auto h = gen_holder(node, seg, is_string_placeholder);
        if (h) {
            method_param_holders.push_back(h);
        }
    }

    param_holder->add_placeholder(method_param_holders);
    return 0;
}

int32_t AuthConfig::parse_check_param(const YAML::Node & node, std::shared_ptr<CheckConfig> check, const std::string & method_param) {
    size_t i = 0;
    std::vector<std::shared_ptr<PlaceHolder>> method_param_holders;
    std::string seg;
    bool is_string_placeholder = true;

    while (i < method_param.size()) {
        if (i + 1 < method_param.size()) {
            if (method_param[i] == '$' && method_param[i] == '{') {
                if (!seg.empty()) {
                    auto h = gen_holder(node, seg, is_string_placeholder);
                    if (h) {
                        method_param_holders.push_back(h);
                    }
                    seg = "";
                }
                i+=2;
                is_string_placeholder = false;
                while (i < method_param.size() && method_param[i] != '}') {
                    seg.append(1, method_param[i]);
                    i++;
                }
    
                auto h = gen_holder(node, seg, is_string_placeholder);
                if (h) {
                    method_param_holders.push_back(h);
                }
                seg = "";
            } else {
                is_string_placeholder = true;
                seg.append(1, method_param[i]);
            }
        } else {
            is_string_placeholder = true;
            seg.append(1, method_param[i]);
        }
        
        i++;
    }

    if (!seg.empty()) {
        auto h = gen_holder(node, seg, is_string_placeholder);
        if (h) {
            method_param_holders.push_back(h);
        }
    }

    check->add_placeholder(method_param_holders);
    return 0;
}

std::shared_ptr<PlaceHolder> AuthConfig::gen_holder(const YAML::Node & node, const std::string & seg, bool is_string_holder) {
    if (is_string_holder) {
        auto placeholder = std::make_shared<StringPlaceHolder>(seg);
        return placeholder;
    }

    if (seg == "domain") {
        auto placeholder = std::make_shared<DomainPlaceHolder>();
        return placeholder;
    } else if (seg == "app") {
        auto placeholder = std::make_shared<AppPlaceHolder>();
        return placeholder;
    } else if (seg == "stream_name") {
        auto placeholder = std::make_shared<StreamPlaceHolder>();
        return placeholder;
    } else if (seg == "stream_type") {
        auto placeholder = std::make_shared<StreamTypePlaceHolder>();
        return placeholder;
    } else if (boost::starts_with(seg, "params[") && boost::ends_with(seg, "]")) {
        std::string param_name = seg.substr(7, seg.size() - 8);//sizeof(params[) = 7
        auto it_param = params_.find(param_name);
        if (it_param == params_.end()) {
            // 如果没有找到，可能是因为参数排列顺序，迭代生成其他参数节点先
            auto param = recursive_search_param_node(node, param_name);
            if (!param) {
                return nullptr;
            }
            return std::make_shared<ParamPlaceHolder>(param_name, param);
        }

        auto placeholder = std::make_shared<ParamPlaceHolder>(param_name, it_param->second);
        return placeholder;
    } else if (boost::starts_with(seg, "url_params[") && boost::ends_with(seg, "]")) {
        std::string param_name = seg.substr(11, seg.size() - 12);//sizeof(url_params[) = 11
        auto placeholder = std::make_shared<UrlParamPlaceHolder>(param_name);
        return placeholder;
    }
    return nullptr;
}

int32_t AuthConfig::check(std::shared_ptr<StreamSession> s) {
    for (auto & check : checks_) {
        if (!check->check(*s)) {
            return -1;
        }
    }
    return 0;
}