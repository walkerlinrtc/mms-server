#pragma once
#include <string_view>
#include <string>
#include <vector>
// a=inactive

//     This specifies that the tools should be started in inactive
//     mode.  This is necessary for interactive conferences where
//     users can put other users on hold.  No media is sent over an
//     inactive media stream.  Note that an RTP-based system SHOULD
//     still send RTCP, even if started inactive.  It can be either a
//     session or media-level attribute, and it is not dependent on
//     charset.
namespace mms {
struct InactiveAttr {
static std::string prefix = "a=inactive";
public:
};
};