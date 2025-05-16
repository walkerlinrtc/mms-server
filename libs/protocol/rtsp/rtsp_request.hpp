#pragma once
#include <string>
#include <unordered_map>
#include <sstream>
#include <boost/algorithm/string.hpp>

#include "rtsp_define.hpp"

namespace mms {
class RtspRequest {
public:
    std::string method_;
    std::string abs_uri_;
    std::unordered_map<std::string, std::string> query_params_;
    std::unordered_map<std::string, std::string> headers_; 
    std::string version_;

    std::string body_;
public:
    RtspRequest();
    virtual ~RtspRequest();
    const std::string & get_method() const;
    void set_method(const std::string & val);
    const std::string & get_uri() const;
    void set_uri(const std::string & val);
    const std::string & get_header(const std::string & k) const;
    void add_header(const std::string & k, const std::string & v);
    const std::string & get_query_param(const std::string & k) const;
    void add_query_param(const std::string & k, const std::string & v);
    void set_query_params(const std::unordered_map<std::string, std::string> & query_params);
    void clear();
    const std::string & get_version();
    void set_version(const std::string & v);
    void set_body(const std::string & body);
    const std::string & get_body() const;
    std::string to_req_string() const;
    int32_t parse(const std::string_view & buf);
private:
    int32_t parse_rtsp_header(const std::string_view & buf);
    int32_t parse_request_line(const std::string_view & buf);
    int32_t parse_headers(const std::string_view & buf);
    int32_t parse_header(const std::string_view & buf);
};

};