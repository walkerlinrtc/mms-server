#pragma once
#include <string_view>
#include <string>
#include <vector>
// a=sendonly

//     This specifies that the tools should be started in send-only
//     mode.  An example may be where a different unicast address is
//     to be used for a traffic destination than for a traffic source.
//     In such a case, two media descriptions may be used, one
//     sendonly and one recvonly.  It can be either a session- or
//     media-level attribute, but would normally only be used as a
//     media attribute.  It is not dependent on charset.  Note that
//     sendonly applies only to the media, and any associated control
//     protocol (e.g., RTCP) SHOULD still be received and processed as
//     normal.
namespace mms
{
    struct SendOnlyAttr
    {
    public:
        static std::string prefix;
        bool parse(const std::string &line);
        std::string to_string() const;
    };
};