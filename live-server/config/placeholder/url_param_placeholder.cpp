#include "url_param_placeholder.h"
#include "../auth/auth_config.h"
#include "core/stream_session.hpp"

using namespace mms;

UrlParamPlaceHolder::UrlParamPlaceHolder(const std::string & name) : name_(name) {
    holder_ = name;
}

UrlParamPlaceHolder::~UrlParamPlaceHolder() {

}

std::string UrlParamPlaceHolder::get_val(StreamSession & session) {
    auto v = session.get_param(name_);
    if (!v.has_value()) {
        return "";
    }
    return v.value();
}