#pragma once
#include <memory>

#include "stun_define.hpp"
#include "stun_msg.h"
#include "stun_error_code_attr.h"
namespace mms {
struct StunBindingErrorResponseMsg : public StunMsg {
public:
    StunBindingErrorResponseMsg(StunMsg & stun_msg, int32_t code, const std::string & msg) {
        header.type = STUN_BINDING_ERROR_RESPONSE;
        memcpy(header.transaction_id, stun_msg.header.transaction_id, 16);
        auto attr = std::make_unique<StunErrorCodeAttr>(code, msg);
        attrs.emplace_back(std::move(attr));
    }

    virtual ~StunBindingErrorResponseMsg() = default;
};
};