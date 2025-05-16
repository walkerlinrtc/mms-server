#include <boost/algorithm/string.hpp>
#include <string_view>
#include <boost/utility/string_view.hpp>
#include <utility>

#include "placeholder_generator.h"
#include "param/param.h"

#include "placeholder/domain_placeholder.h"
#include "placeholder/app_placeholder.h"
#include "placeholder/stream_name_placeholder.h"
#include "placeholder/stream_type_placeholder.h"
#include "placeholder/string_placeholder.h"
#include "placeholder/param_placeholder.h"
#include "placeholder/url_param_placeholder.h"
#include "placeholder/header_param_placeholder.h"

#include "spdlog/spdlog.h"

using namespace mms;

int32_t PlaceHolderGenerator::parse_string_to_placeholders(const YAML::Node & node, const std::string & unformatte_str, std::vector<std::shared_ptr<PlaceHolder>> & holders) {
    size_t i = 0;
    std::string seg;
    bool is_string_placeholder = true;
    std::map<std::string, std::shared_ptr<Param>> params;
    while (i < unformatte_str.size()) {
        if (i + 1 < unformatte_str.size()) {
            if (unformatte_str[i] == '$' && unformatte_str[i+1] == '{') {
                if (!seg.empty()) {
                    auto h = PlaceHolderGenerator::gen_holder(node, seg, is_string_placeholder, params);
                    if (h) {
                        holders.push_back(h);
                    }
                    seg = "";
                }

                i+=2;
                is_string_placeholder = false;
                while (i < unformatte_str.size() && unformatte_str[i] != '}') {
                    seg.append(1, unformatte_str[i]);
                    i++;
                }

                auto h = PlaceHolderGenerator::gen_holder(node, seg, is_string_placeholder, params);
                if (h) {
                    holders.push_back(h);
                }
                seg = "";
            } else {
                is_string_placeholder = true;
                seg.append(1, unformatte_str[i]);
            }
        } else {
            is_string_placeholder = true;
            seg.append(1, unformatte_str[i]);
        }
        i++;
    }

    if (!seg.empty()) {
        auto h = PlaceHolderGenerator::gen_holder(node, seg, is_string_placeholder, params);
        if (h) {
            holders.push_back(h);
        }
    }
    
    return 0;
}


int32_t PlaceHolderGenerator::parse_method_param(const YAML::Node & node, std::shared_ptr<Param> param_holder, const std::string & method_param, std::map<std::string, std::shared_ptr<Param>> & params) {
    size_t i = 0;
    std::vector<std::shared_ptr<PlaceHolder>> method_param_holders;
    std::string seg;
    bool is_string_placeholder = true;
    while (i < method_param.size()) {
        if (i + 1 < method_param.size()) {
            if (method_param[i] == '$' && method_param[i+1] == '{') {
                if (!seg.empty()) {
                    auto h = PlaceHolderGenerator::gen_holder(node, seg, is_string_placeholder, params);
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
                auto h = PlaceHolderGenerator::gen_holder(node, seg, is_string_placeholder, params);
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
        auto h = PlaceHolderGenerator::gen_holder(node, seg, is_string_placeholder, params);
        if (h) {
            method_param_holders.push_back(h);
        }
    }

    param_holder->add_placeholder(method_param_holders);
    return 0;
}


std::shared_ptr<PlaceHolder> PlaceHolderGenerator::gen_holder(const YAML::Node & node, 
                                                              std::string & seg, bool is_string_holder, 
                                                              std::map<std::string, std::shared_ptr<Param>> & params) {
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
        auto it_param = params.find(param_name);
        if (it_param == params.end()) {
            // 如果没有找到，可能是因为参数排列顺序，迭代生成其他参数节点先
            auto param = PlaceHolderGenerator::recursive_search_param_node(node, param_name, params);
            if (!param) {
                spdlog::error("could not search param:{}", param_name);
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
    } else if (boost::starts_with(seg, "header_params[") && boost::ends_with(seg, "]")) {
        std::string param_name = seg.substr(14, seg.size() - 15);//sizeof(header_params[) = 14
        auto placeholder = std::make_shared<HeaderParamPlaceHolder>(param_name);
        return placeholder;
    }
    return nullptr;
}


std::shared_ptr<Param> PlaceHolderGenerator::recursive_search_param_node(const YAML::Node & node, 
                                                                         const std::string & param_name, 
                                                                         std::map<std::string, std::shared_ptr<Param>> & params) {
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
                std::shared_ptr<Param> param = Param::gen_param(method_name);
                if (!param) {
                    return nullptr;
                }
                std::string method_params_list = param_pattern.substr(left_bracket_pos + 1, param_pattern.size() - left_bracket_pos - 2);
                std::vector<std::string> method_params;
                boost::split(method_params, method_params_list, boost::is_any_of(","));
                for (size_t i = 0; i < method_params.size(); i++) {
                    if (0 != parse_method_param(node, param, method_params[i], params)) {
                        return nullptr;
                    }
                }
                params.insert(std::pair(name, param));
                return param;
            }
        }
    }
    return nullptr;
}