#include "greater_check_config.h"
#include "core/stream_session.hpp"
#include "auth_config.h"

using namespace mms;

GreaterCheckConfig::GreaterCheckConfig(bool can_equal) : can_equal_(can_equal) {

}

GreaterCheckConfig::~GreaterCheckConfig() {

}

bool GreaterCheckConfig::check(StreamSession & session, const std::vector<std::string> & method_params) {
    ((void)session);
    if (method_params.size() != 2) {
        return false;
    }

    try {
        auto i1 = std::atoll(method_params[0].c_str());
        auto i2 = std::atoll(method_params[1].c_str());
        spdlog::info("i1:{}, i2:{}", i1, i2);
        if (can_equal_) {
            return i1 >= i2;
        } else {
            return i1 > i2;
        }
    } catch (std::exception & e) {
        return false;
    }
}

