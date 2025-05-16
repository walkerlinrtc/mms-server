#include "check_config.h"
#include "server/session.hpp"
#include "auth_config.h"
#include "equal_check_config.h"
#include "greater_check_config.h"
#include "smaller_check_config.h"
#include "../placeholder/placeholder.h"

using namespace mms;

CheckConfig::CheckConfig() {

}

CheckConfig::~CheckConfig() {

}

std::shared_ptr<CheckConfig> CheckConfig::gen_check(const std::string & method_name) {
    if (method_name == "<") {
        return std::make_shared<SmallerCheckConfig>();
    } else if (method_name == ">") {
        return std::make_shared<GreaterCheckConfig>();
    } else if (method_name == "==") {
        return std::make_shared<EqualCheckConfig>();
    } else if (method_name == "<=") {
        return std::make_shared<SmallerCheckConfig>(true);
    } else if (method_name == ">=") {
        return std::make_shared<GreaterCheckConfig>(true);
    } else if (method_name == "!=") {
        return std::make_shared<EqualCheckConfig>(false);
    }
    return nullptr;
}

bool CheckConfig::check(StreamSession & session) {
    std::vector<std::string> method_params;
    for (auto & mph : holders_) {
        std::string p;
        for (auto & h : mph) {
            p += h->get_val(session);
        }
        method_params.emplace_back(p);
    }

    return check(session, method_params);
}

void CheckConfig::add_placeholder(const std::vector<std::shared_ptr<PlaceHolder>> & method_param_holder) {
    holders_.push_back(method_param_holder);
}

