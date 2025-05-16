#pragma once
#include <vector>
#include <memory>
#include <string>

#include "check_config.h"

namespace mms {
class PlaceHolder;
class AuthConfig;
class StreamSession;

class GreaterCheckConfig : public CheckConfig {
public:
    GreaterCheckConfig(bool can_equal = false);
    virtual ~GreaterCheckConfig();
public:
    bool check(StreamSession & session, const std::vector<std::string> & method_params);
protected:
    std::vector<std::shared_ptr<PlaceHolder>> holders_;
    bool can_equal_ = false;
};
};