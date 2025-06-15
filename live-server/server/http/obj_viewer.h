#pragma once
#include "json/json.h"
namespace mms {
class ObjViewer {
public:
    ObjViewer() = default;
    virtual ~ObjViewer() = default;
public:
    Json::Value to_json();
};
};