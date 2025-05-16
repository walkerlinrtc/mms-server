#pragma once
#include <string>
#include <string>
#include <vector>
// a=orient:<orientation>

//     Normally this is only used for a whiteboard or presentation
//     tool.  It specifies the orientation of a the workspace on the
//     screen.  It is a media-level attribute.  Permitted values are
//     "portrait", "landscape", and "seascape" (upside-down
//     landscape).  It is not dependent on charset.
namespace mms {
struct OrientAttr {
static std::string prefix = "a=orient:";
public:
    std::string orientation;
};
};