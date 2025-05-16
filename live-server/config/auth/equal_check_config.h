#pragma once
#include <vector>
#include <memory>
#include <string>

#include "check_config.h"

namespace mms {
class PlaceHolder;
class AuthConfig;
class StreamSession;

class EqualCheckConfig : public CheckConfig {
public:
    EqualCheckConfig(bool equal = true);
    virtual ~EqualCheckConfig();
public:
    bool check(StreamSession & session, const std::vector<std::string> & method_params);
protected:
    bool equal_ = true;
    std::vector<std::shared_ptr<PlaceHolder>> holders_;
};
};