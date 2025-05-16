#pragma once
#include "placeholder.h"
namespace mms {
class StreamSession;
class AppPlaceHolder : public PlaceHolder {
public:
    AppPlaceHolder();
    virtual ~AppPlaceHolder();
public:
    std::string get_val(StreamSession & session) final;
};
};