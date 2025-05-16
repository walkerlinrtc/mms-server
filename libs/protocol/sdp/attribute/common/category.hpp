#pragma once
#include <string_view>
#include <string>
// a=cat:<category>
//          This attribute gives the dot-separated hierarchical category of
//          the session.  This is to enable a receiver to filter unwanted
//          sessions by category.  There is no central registry of
//          categories.  It is a session-level attribute, and it is not
//          dependent on charset.
namespace mms {
struct CategoryAttr {
static std::string prefix = "a=cat:";
public:
    std::string category;
};
};