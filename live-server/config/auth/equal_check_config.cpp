#include "equal_check_config.h"
#include "core/stream_session.hpp"
#include "auth_config.h"
#include "log/log.h"

using namespace mms;

EqualCheckConfig::EqualCheckConfig(bool equal) : equal_(equal) {

}

EqualCheckConfig::~EqualCheckConfig() {

}

bool EqualCheckConfig::check(StreamSession & session, const std::vector<std::string> & method_params) {
    ((void)session);
    if (method_params.size() != 2) {
        return false;
    }

    if (equal_) {
        CORE_DEBUG("equal check config, param0:{} == param1:{}", method_params[0], method_params[1]);
        return method_params[0] == method_params[1];
    } else {
        CORE_DEBUG("equal check config, param0:{} != param1:{}", method_params[0], method_params[1]);
        return method_params[0] != method_params[1];
    }
}

