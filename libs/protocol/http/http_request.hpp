#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include "http_define.h"

namespace mms {
class SocketInterface;
class HttpRequest {
public:
    HttpRequest();
    virtual ~HttpRequest();
    HTTP_METHOD get_method() const;
    void set_method(HTTP_METHOD val);
    void set_method(const std::string & val);
    const std::string & get_path() const;
    void set_path(const std::string & path);
    const std::string & get_header(const std::string & k) const;
    const std::unordered_map<std::string, std::string> & get_headers() const;
    void add_header(const std::string & k, const std::string & v);
    const std::string & get_query_param(const std::string & k) const;
    const std::unordered_map<std::string, std::string> & get_query_params() const;
    void add_query_param(const std::string & k, const std::string & v);
    void set_query_params(const std::unordered_map<std::string, std::string> & query_params);
    // path路径参数 /:app/:stream  app和stream就是路径参数
    const std::string & get_path_param(const std::string & k) const;
    void add_path_param(const std::string & k, const std::string & v);
    std::unordered_map<std::string, std::string> & path_params();
    void set_scheme(const std::string & scheme);
    const std::string & get_scheme() const;
    const std::string & get_version();
    void set_version(const std::string & v);
    const std::string & get_version() const;
    void set_body(const std::string & body);
    const std::string & get_body() const;
    std::string to_req_string() const;
    bool parse_request_line(const char *buf, size_t len);
    bool parse_header(const char *buf, size_t len);
    int32_t parse_body(const char *buf, size_t len);
protected:
    std::unordered_map<HTTP_METHOD, std::string> method_map_ = {
        {GET, "GET"},
        {POST, "POST"},
        {HEAD, "HEAD"},
        {PATCH, "PATCH"},
        {OPTIONS, "OPTIONS"},
        {PUT, "PUT"},
        {DELETE, "DELETE"}
    };
protected:
    std::shared_ptr<SocketInterface> conn_;
public:
    HTTP_METHOD method_;
    std::string method_str_;
    std::string path_;
    std::unordered_map<std::string, std::string> query_params_;
    std::unordered_map<std::string, std::string> path_params_;
    std::unordered_map<std::string, std::string> headers_;
    std::string scheme_;
    std::string version_;

    std::string body_;
    void * extra_ = nullptr;
};

};