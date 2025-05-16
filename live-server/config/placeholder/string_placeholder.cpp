#include "string_placeholder.h"
using namespace mms;

StringPlaceHolder::StringPlaceHolder(const std::string & v) : val_(v) {
    holder_ = v;
}

StringPlaceHolder::~StringPlaceHolder() {

}

std::string StringPlaceHolder::get_val(StreamSession & session) {
    ((void)session);
    return val_;
}