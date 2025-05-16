#include "stream_type_placeholder.h"
#include "core/stream_session.hpp"

using namespace mms;
StreamTypePlaceHolder::StreamTypePlaceHolder() {
    holder_ = "stream_type";
}

StreamTypePlaceHolder::~StreamTypePlaceHolder() {

}

std::string StreamTypePlaceHolder::get_val(StreamSession & session) {
    return session.get_session_type();
}