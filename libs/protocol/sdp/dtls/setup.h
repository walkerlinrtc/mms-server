#pragma once
// https://tools.ietf.org/html/rfc4145#section-4
// a=setup 主要是表示dtls的协商过程中角色的问题，谁是客户端，谁是服务器
// a=setup:actpass 既可以是客户端，也可以是服务器
// a=setup:active 客户端
// a=setup:passive 服务器
#include <string>
namespace mms {
#define ROLE_ACTIVE      "active"
#define ROLE_PASSIVE     "passive"
#define ROLE_ACTPASS     "actpass"
#define ROLD_HOLDCONN    "holdconn"

struct SetupAttr {
public:
    static std::string prefix;
    SetupAttr() = default;
    SetupAttr(const std::string & role) : role_(role) {
        
    }
    bool parse(const std::string & line);
    std::string to_string() const;
    const std::string & get_role() const {
        return role_;
    }

    void set_role(const std::string &val) {
        role_ = val;
    }
public:
    std::string role_;
};
};