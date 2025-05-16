#include "domain_placeholder.h"
#include "core/stream_session.hpp"

using namespace mms;
DomainPlaceHolder::DomainPlaceHolder() {
    holder_ = "domain";
}

DomainPlaceHolder::~DomainPlaceHolder() {

}

std::string DomainPlaceHolder::get_val(StreamSession & session) {
    return session.get_domain_name();
}