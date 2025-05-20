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
#include "../placeholder_generator.h"

using namespace mms;
int32_t AuthConfig::load(const YAML::Node & node) {
    YAML::Node enabled = node["enabled"];
    if (enabled.IsDefined()) {
        enabled_ = enabled.as<bool>();
    } else {
        enabled_ = false;
    }

    auto checks_node = node["checks"];
    if (!checks_node.IsDefined() || !checks_node.IsSequence()) {
        return -6;
    }

    for (size_t i = 0; i < checks_node.size(); i++) {
        YAML::Node check_node = checks_node[i];
        if (0 != parse_check_config(check_node, node)) {
            return -7;
        }
    }

    return 0;
}

int32_t AuthConfig::parse_check_config(const YAML::Node & node, const YAML::Node & parent_node) {
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

    std::vector<std::shared_ptr<PlaceHolder>> holders1;
    auto ret = PlaceHolderGenerator::parse_string_to_placeholders(parent_node, param1, holders1);
    if (0 != ret) {
        return -4;
    }
    check->add_placeholder(holders1);

    std::vector<std::shared_ptr<PlaceHolder>> holders2;
    ret = PlaceHolderGenerator::parse_string_to_placeholders(parent_node, param2, holders2);
    if (0 != ret) {
        return -5;
    }
    check->add_placeholder(holders2);
    
    checks_.push_back(check);
    return 0;
}

int32_t AuthConfig::check(std::shared_ptr<StreamSession> s) {
    for (auto & check : checks_) {
        if (!check->check(*s)) {
            return -1;
        }
    }
    return 0;
}