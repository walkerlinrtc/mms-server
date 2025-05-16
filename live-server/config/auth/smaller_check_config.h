#pragma once
#include <vector>
#include <memory>
#include <string>
#include "check_config.h"

namespace mms {
class PlaceHolder;
class AuthConfig;
class StreamSession;

class SmallerCheckConfig : public CheckConfig {
public:
    SmallerCheckConfig(bool can_equal = false);
    virtual ~SmallerCheckConfig();
public:
    bool check(StreamSession & session, const std::vector<std::string> & method_params);
protected:
    bool can_equal_ = false;
    std::vector<std::shared_ptr<PlaceHolder>> holders_;
};
};