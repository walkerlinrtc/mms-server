#pragma once
#include <string>
#include <string>
#include <vector>
namespace mms
{
    struct RecvOnlyAttr
    {
    public:
        static std::string prefix;
        bool parse(const std::string &line);
        std::string to_string() const;
    };
};