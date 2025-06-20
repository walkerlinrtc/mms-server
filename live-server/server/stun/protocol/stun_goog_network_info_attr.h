#pragma once
#include "stun_define.hpp"
namespace mms {
struct StunGoogNetworkInfoAttr : public StunMsgAttr
{
    StunGoogNetworkInfoAttr(const std::string & username) : StunMsgAttr(STUN_ATTR_GOOG_NETWORK_INFO)
    {
        (void)username;
    }

    StunGoogNetworkInfoAttr() = default;
    virtual ~StunGoogNetworkInfoAttr() = default;

    size_t size();

    int32_t encode(uint8_t *data, size_t len);

    int32_t decode(uint8_t *data, size_t len);
private:
    uint16_t network_id_;
    uint16_t network_cost_;
};
};