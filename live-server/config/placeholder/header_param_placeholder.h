#pragma once
#include <string>
#include "placeholder.h"
namespace mms {
class AuthConfig;
class StreamSession;
class HeaderParamPlaceHolder : public PlaceHolder {
public:
    HeaderParamPlaceHolder(const std::string & name);
    virtual ~HeaderParamPlaceHolder();
public:
    std::string get_val(StreamSession & session);
private:
    std::string name_;
};
};