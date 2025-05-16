#pragma once
#include <vector>
#include <memory>
#include <string>

namespace mms {
class PlaceHolder;
class AuthConfig;
class StreamSession;

class CheckConfig {
public:
    CheckConfig();
    virtual ~CheckConfig();
    static std::shared_ptr<CheckConfig> gen_check(const std::string & method_name);
public:
    virtual bool check(StreamSession & session);
    virtual bool check(StreamSession & session, const std::vector<std::string> & method_params) = 0;
    void add_placeholder(const std::vector<std::shared_ptr<PlaceHolder>> & method_param_holder);
protected:
    std::vector<std::vector<std::shared_ptr<PlaceHolder>>> holders_;
};
};