#pragma once
#include <string>
namespace mms
{
    struct Control
    {
    public:
        Control(const std::string & val) {
            val_ = val;
        }
        Control() = default;
        static std::string prefix;
        bool parse(const std::string &line);
        
        const std::string get_val() const {
            return val_;
        }
        std::string to_string() const;

    protected:
        std::string val_;
    };
};