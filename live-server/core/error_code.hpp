#pragma once
namespace mms {
struct Error {
    int32_t code;
    std::string msg;
};

#define ERROR_SUCCESS       0
#define ERROR_FORBIDDEN     403
#define ERROR_SERVER_ERROR  500
};