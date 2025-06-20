#pragma once
#include "stun_define.hpp"

namespace mms {
struct StunBindingRequestMsg : public StunMsg {
public:
    virtual ~StunBindingRequestMsg() = default;
};
};