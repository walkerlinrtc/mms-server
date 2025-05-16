/*
 * @Author: jbl19860422
 * @Date: 2023-08-03 22:13:23
 * @LastEditTime: 2023-09-16 11:17:19
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\config\source_config.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include <boost/algorithm/string.hpp>
#include <string_view>
#include <string>
#include <boost/utility/string_view.hpp>

#include "origin_pull_config.h"
#include "../placeholder/domain_placeholder.h"
#include "../placeholder/app_placeholder.h"
#include "../placeholder/stream_name_placeholder.h"
#include "../placeholder/stream_type_placeholder.h"
#include "../placeholder/string_placeholder.h"
#include "../placeholder/param_placeholder.h"
#include "../placeholder/url_param_placeholder.h"

#include "../param/param.h"
#include "../param/md5_param.h"
#include "service/dns/dns_service.hpp"

#include "spdlog/spdlog.h"
#include "../placeholder_generator.h"

using namespace mms;
OriginPullConfig::OriginPullConfig() {

}

OriginPullConfig::~OriginPullConfig() {

}

int32_t OriginPullConfig::load(const YAML::Node & node) {
    auto proto_node = node["protocol"];
    if (!proto_node.IsDefined()) {
        return -1;
    }
    protocol_ = proto_node.as<std::string>();

    auto url_node = node["url"];
    if (!url_node.IsDefined()) {
        return -2;
    }

    unformat_url_ = url_node.as<std::string>();
    if (0 != PlaceHolderGenerator::parse_string_to_placeholders(node, unformat_url_, url_holders_)) {
        return -3;
    }

    auto no_players_timeout = node["no_players_timeout"];
    try {
        if (no_players_timeout.IsDefined()) {
            auto sno_players_timeout = no_players_timeout.as<std::string>();
            if (sno_players_timeout.ends_with("ms")) {
                auto s = sno_players_timeout.substr(0, sno_players_timeout.size() - 2);
                no_players_timeout_ms_ = std::atoi(s.c_str());
            } else if (sno_players_timeout.ends_with("s")) {
                auto s = sno_players_timeout.substr(0, sno_players_timeout.size() - 1);
                no_players_timeout_ms_ = std::atoi(s.c_str())*1000;
            }
        }
    } catch (std::exception & exp) {
        spdlog::error("read no_players_timeout failed");
        return -4;
    }

    return 0;
}

std::string OriginPullConfig::gen_url(std::shared_ptr<StreamSession> s) {
    std::string url;
    for (auto & h : url_holders_) {
        url += h->get_val(*s);
    }
    return url;
}