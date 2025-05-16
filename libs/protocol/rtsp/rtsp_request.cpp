#include "rtsp_request.hpp"
using namespace mms;
RtspRequest::RtspRequest() {
}

RtspRequest::~RtspRequest() {

}

const std::string & RtspRequest::get_method() const {
    return method_;
}

void RtspRequest::set_method(const std::string & val) {
    method_ = val;
}

const std::string & RtspRequest::get_uri() const {
    return abs_uri_;
}

void RtspRequest::set_uri(const std::string & val) {
    abs_uri_ = val;
}

const std::string & RtspRequest::get_header(const std::string & k) const {
    static std::string empty_str("");
    auto it = headers_.find(k);
    if (it == headers_.end()) {
        return empty_str;
    }
    return it->second;
}

void RtspRequest::add_header(const std::string & k, const std::string & v) {
    headers_[k] = v;
}

const std::string & RtspRequest::get_query_param(const std::string & k) const {
    static std::string empty_str("");
    auto it = query_params_.find(k);
    if (it == query_params_.end()) {
        return empty_str;
    }
    return it->second;
}

void RtspRequest::add_query_param(const std::string & k, const std::string & v) {
    query_params_[k] = v;
}

void RtspRequest::set_query_params(const std::unordered_map<std::string, std::string> & query_params) {
    query_params_ = query_params;
}

void RtspRequest::clear() {
    method_ = "";
    abs_uri_ = "";
    query_params_.clear();
    headers_.clear();
}

const std::string & RtspRequest::get_version() {
    return version_;
}

void RtspRequest::set_version(const std::string & v) {
    version_ = v;
}

void RtspRequest::set_body(const std::string & body) {
    body_ = body;
}

const std::string & RtspRequest::get_body() const {
    return body_;
}

std::string RtspRequest::to_req_string() const {
    std::ostringstream oss;
    oss << method_ << " " << abs_uri_;
    if (query_params_.size() > 0) {
        oss << "?";
        size_t i = 0;
        for (auto & param : query_params_) {
            if (i == query_params_.size() - 1) {
                oss << param.first << "=" << param.second;
            } else {
                oss << param.first << "=" << param.second << "&";
            }
            i++;
        }
    }
    oss << " RTSP/" << version_ << RTSP_CRLF;
    if (headers_.size() > 0) {
        for (auto & p : headers_) {
            oss << p.first << ": " << p.second << RTSP_CRLF;
        }
    }
    oss << RTSP_CRLF;
    if (body_.size() > 0) {
        oss << body_;
    }
    return oss.str();
}

int32_t RtspRequest::parse(const std::string_view & buf) {
    std::string_view buf_inner = buf;
    auto consumed_header = parse_rtsp_header(buf_inner);
    if (consumed_header <= 0) {
        return consumed_header;
    }
    buf_inner.remove_prefix(consumed_header);
    auto it_content_len = headers_.find("Content-Length");
    int32_t content_len = 0;
    if (it_content_len != headers_.end()) {
        try {
            content_len = std::atoi(it_content_len->second.c_str());
        } catch (std::exception & e) {
            return -1;
        }

        if (content_len < 0) {
            return -2;
        }
    }

    if (content_len > 0) {
        if ((int32_t)buf_inner.size() < content_len) {
            return 0;//不足，等一整个完整包
        }
        body_.assign(buf_inner.data(), content_len);
        return consumed_header + content_len;
    }
    return consumed_header;
}

int32_t RtspRequest::parse_rtsp_header(const std::string_view & buf) {
    auto pos_end = buf.find(RTSP_HEADER_DIVIDER);
    if (pos_end == std::string_view::npos) {
        return 0;
    }
    // 扫描请求行
    std::string_view buf_inner(buf.data(), pos_end + 4);
    auto consumed = parse_request_line(buf_inner);
    if (consumed < 0) {
        return -1;
    }
    buf_inner.remove_prefix(consumed);
    // 扫描属性行
    consumed = parse_headers(buf_inner);
    if (consumed < 0) {
        return -2;
    }
    //结束
    return pos_end + 4;
}

#include "spdlog/spdlog.h"
int32_t RtspRequest::parse_request_line(const std::string_view & buf) {
    std::string_view buf_inner = buf;
    auto pos_end = buf_inner.find(RTSP_CRLF);
    if (pos_end == std::string_view::npos) {
        return -1;
    }
    auto s = buf_inner.substr(0, pos_end);
    //METHOD
    auto pos = s.find(" ");
    if (pos == std::string_view::npos) {
        return -2;
    }
    method_ = s.substr(0, pos);
    s.remove_prefix(pos + 1);
    // Request-URI
    pos = s.find(" ");
    if (pos == std::string_view::npos) {
        return -4;
    }
    std::string_view uri = s.substr(0, pos);
    auto pos1 = uri.find("?");
    if (pos1 != std::string::npos) {
        abs_uri_ = uri.substr(0, pos1);
        std::string_view params = uri.substr(pos1+1);
        std::vector<std::string> vsp;
        boost::split(vsp, params, boost::is_any_of("&"));
        for(auto & s : vsp) {
            auto equ_pos = s.find("=");
            if (equ_pos == std::string::npos) {
                continue;
            }

            std::string name = s.substr(0, equ_pos);
            std::string value = s.substr(equ_pos + 1);
            query_params_[name] = value;
        }
    } else {
        abs_uri_ = uri;
    }
    
    s.remove_prefix(pos + 1);
    //RTSP-Version
    if (s.size() <= 0) {
        return -7;
    }
    pos = s.find("/");
    if (pos == std::string_view::npos) {
        return -8;
    }
    std::string_view rtsp = s.substr(0, pos);
    if (rtsp != "RTSP") {
        return -9;
    }
    s.remove_prefix(pos + 1);
    if (s.size() <= 0) {
        return -10;
    }
    version_ = s;
    return pos_end + 2;
}

int32_t RtspRequest::parse_headers(const std::string_view & buf) {
    std::string_view buf_inner(buf);
    int32_t total_consumed = 0;
    while (1) {
        int32_t consumed = parse_header(buf_inner);
        if (consumed < 0) {
            return -1;
        }
        
        buf_inner.remove_prefix(consumed);
        total_consumed += consumed;
        if (consumed == 2) {
            break;
        }
    }
    return total_consumed;
}

int32_t RtspRequest::parse_header(const std::string_view & buf) {
    auto pos_end = buf.find(RTSP_CRLF);
    if (pos_end == std::string_view::npos) {
        return -1;
    }

    if (pos_end == 0) {//结束
        return pos_end + 2;
    }

    auto s = buf.substr(0, pos_end);
    auto pos_comma = s.find(": ");
    if (pos_comma == std::string_view::npos) {
        return -2;
    }
    std::string attr(s.substr(0, pos_comma));
    std::string val(s.substr(pos_comma + 2));
    headers_[attr] = val;
    return pos_end + 2;
}