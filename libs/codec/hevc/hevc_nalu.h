#pragma once
#include <stdint.h>
#include <string>
#include <string_view>

namespace mms {
class HEVCNALU {
public:
    uint8_t nalu_header;//F + NRI + TYPE
    std::string rbsp;
    std::string_view get_data() {
        return data;
    }
private:
    std::string_view data;
};

};