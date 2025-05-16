#pragma once
#include <string_view>
#include <string>
// a=keywds:<keywords>

//     Like the cat attribute, this is to assist identifying wanted
//     sessions at the receiver.  This allows a receiver to select
//     interesting session based on keywords describing the purpose of
//     the session; there is no central registry of keywords.  It is a
//     session-level attribute.  It is a charset-dependent attribute,
//     meaning that its value should be interpreted in the charset
//     specified for the session description if one is specified, or
//     by default in ISO 10646/UTF-8.
namespace mms {
struct KeywdsAttr {
static std::string prefix = "a=keywds:";
public:
    std::string keywords;
};
};