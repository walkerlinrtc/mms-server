#pragma once
#include <stdint.h>
#include <string>
#include <string>

namespace mms {
struct ProtocolVersion {
public:
    static std::string prefix;
    bool parse(const std::string & line);
    int32_t get_version();
    void set_version(int v);
    std::string to_string() const;
private:
    int version;
};
};