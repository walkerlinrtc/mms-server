#pragma once
#include <unordered_map>
#include <string>
namespace mms {
#define HTTP_MAX_BUF (1024*8)
// http method http1.1-rfc2616.txt @5.1.1
#define HTTP_METHOD_OPTIONS     "OPTIONS"
#define HTTP_METHOD_GET         "GET"
#define HTTP_METHOD_HEAD        "HEAD"
#define HTTP_METHOD_POST        "POST"
#define HTTP_METHOD_PUT         "PUT"
#define HTTP_METHOD_DELETE      "DELETE"
#define HTTP_METHOD_TRACE       "TRACE"
#define HTTP_METHOD_CONNECT     "CONNECT"

enum HTTP_METHOD {
    CONNECT     = 0,
    GET         = 1,
    HEAD        = 2,
    POST        = 3,
    PUT         = 4,
    DELETE      = 5,
    TRACE       = 6,
    OPTION      = 7,
};

#define HTTP_CRLF               "\r\n"
#define HTTP_HEADER_DIVIDER     "\r\n\r\n"
#define HTTP_VERSION_1_0        "HTTP/1.0"
#define HTTP_VERSION_1_1        "HTTP/1.1"

extern std::unordered_map<std::string, std::string> http_content_type_map;
};