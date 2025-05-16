#include "stream_name_placeholder.h"
#include "core/stream_session.hpp"

using namespace mms;
StreamPlaceHolder::StreamPlaceHolder() {
    holder_ = "stream_name";
}

StreamPlaceHolder::~StreamPlaceHolder() {

}

std::string StreamPlaceHolder::get_val(StreamSession & session) {
    return session.get_stream_name();
}