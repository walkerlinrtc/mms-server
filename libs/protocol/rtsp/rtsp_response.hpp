#pragma once
#include <unordered_map>
#include <string>

#include <boost/asio/spawn.hpp>

#include "rtsp_define.hpp"

namespace mms {
class RtspResponse {
public:
    RtspResponse();
    virtual ~RtspResponse();
    void add_header(const std::string & k, const std::string & v);
    void set_status(int code, const std::string & reason);
    void set_body(const std::string & body);
    const std::string & get_body() const;
    std::string to_string() const;
    const std::string & get_version() const;
    int32_t get_status_code() const;
    const std::string & get_msg() const;
    int32_t parse(const std::string_view buf);
private:
    int32_t parse_rtsp_header(const std::string_view & buf);
    int32_t parse_status_line(const std::string_view & buf);
    int32_t parse_headers(const std::string_view & buf);
    int32_t parse_header(const std::string_view & buf);
private:
    std::string version_;
    int32_t status_code_ = 200;
    std::string reason_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
};
};