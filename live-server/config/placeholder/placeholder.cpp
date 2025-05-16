#include "placeholder.h"
#include "core/stream_session.hpp"

using namespace mms;
PlaceHolder::PlaceHolder() {

}

PlaceHolder::~PlaceHolder() {

}

std::string PlaceHolder::get_val(StreamSession & session) {
    ((void)session);
    return "";
}