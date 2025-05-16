#pragma once 
#include <string>  
namespace mms
{
    struct RtcpMux
    {
    public:
        static std::string prefix;
        RtcpMux() = default;
        bool parse(const std::string &line);
        std::string to_string() const;
    };
} // namespace mms;
