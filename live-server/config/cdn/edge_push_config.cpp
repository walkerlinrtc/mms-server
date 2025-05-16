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
#include <boost/utility/string_view.hpp>

#include "edge_push_config.h"
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

#include "../placeholder_generator.h"

using namespace mms;
EdgePushConfig::EdgePushConfig() {

}

EdgePushConfig::~EdgePushConfig() {

}

int32_t EdgePushConfig::load(const YAML::Node & node) {
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

    return 0;
}

std::string EdgePushConfig::gen_url(std::shared_ptr<StreamSession> s) {
    std::string url;
    for (auto & h : url_holders_) {
        url += h->get_val(*s);
    }

    if (test_index_ != -1) {
        url += "_" + std::to_string(test_index_);
    }
    return url;
}