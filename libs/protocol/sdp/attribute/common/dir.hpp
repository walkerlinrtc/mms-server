#pragma once
#include <string>
#include <string>
#include <vector>
namespace mms
{
    struct DirAttr
    {
    public:
        enum MEDIA_DIR {
            MEDIA_SENDONLY = 0,
            MEDIA_RECVONLY = 1,
            MEDIA_SENDRECV = 2,
        };
    public:
        DirAttr() {

        }
        
        DirAttr(MEDIA_DIR v) {
            dir_ = v;
        }
        static bool is_my_prefix(const std::string & line);
        bool parse(const std::string &line);
        std::string to_string() const;

        void set_val(MEDIA_DIR val) {
            dir_ = val;
        }

        MEDIA_DIR get_val() const {
            return dir_;
        }
    private:
        MEDIA_DIR dir_;
    };
};