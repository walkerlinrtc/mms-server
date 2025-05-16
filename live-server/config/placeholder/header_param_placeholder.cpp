#include "header_param_placeholder.h"
#include "../auth/auth_config.h"
#include "core/stream_session.hpp"

using namespace mms;

HeaderParamPlaceHolder::HeaderParamPlaceHolder(const std::string & name) : name_(name) {
    holder_ = name;
}

HeaderParamPlaceHolder::~HeaderParamPlaceHolder() {

}

std::string HeaderParamPlaceHolder::get_val(StreamSession & session) {
    auto v = session.get_header(name_);
    if (!v.has_value()) {
        return "";
    }
    return v.value();
}