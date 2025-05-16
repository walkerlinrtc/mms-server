#include "rtsp_response.hpp"
using namespace mms;

RtspResponse::RtspResponse() {
}

RtspResponse::~RtspResponse() {
}

void RtspResponse::add_header(const std::string & k, const std::string & v) {
    headers_[k] = v;
}

void RtspResponse::set_status(int code, const std::string & reason) {
    status_code_ = code;
    reason_ = reason;
}

void RtspResponse::set_body(const std::string & body) {
    body_ = body;
}

const std::string & RtspResponse::get_body() const {
    return body_;
}

std::string RtspResponse::to_string() const {
    std::ostringstream ss;
    ss  << "RTSP/1.0 " << status_code_ << " " << reason_ << "\r\n";
    for(auto & h : headers_) {
        ss << h.first << ": " << h.second << "\r\n";
    }
    
    ss << "\r\n";

    if (body_.size() > 0) {
        ss << body_;
    }
    
    return ss.str();
}

const std::string & RtspResponse::get_version() const {
    return version_;
}

int32_t RtspResponse::get_status_code() const {
    return status_code_;
}

const std::string & RtspResponse::get_msg() const {
    return reason_;
}

int32_t RtspResponse::parse(const std::string_view buf) {
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

int32_t RtspResponse::parse_rtsp_header(const std::string_view & buf) {
    auto pos_end = buf.find(RTSP_HEADER_DIVIDER);
    if (pos_end == std::string_view::npos) {
        return 0;
    }
    // 扫描状态行
    std::string_view buf_inner(buf.data(), pos_end + 4);
    auto consumed = parse_status_line(buf_inner);
    if (consumed < 0) {
        return -1;
    }
    buf_inner.remove_prefix(consumed);
    // 扫描属性行
    consumed = parse_headers(buf_inner);
    if (consumed < 0) {
        return -2;
    }
    buf_inner.remove_prefix(consumed);
    //结束
    return pos_end + 4;
}

int32_t RtspResponse::parse_status_line(const std::string_view & buf) {
    std::string_view buf_inner = buf;
    auto pos_end = buf_inner.find("\r\n");
    if (pos_end == std::string_view::npos) {
        return -1;
    }
    auto s = buf_inner.substr(0, pos_end);
    //version
    auto pos = s.find(" ");
    if (pos == std::string_view::npos) {
        return -2;
    }
    std::string_view h = s.substr(0, pos);
    auto pos2 = h.find("/");
    if (pos2 == std::string_view::npos) {
        return -3;
    }
    version_ = h.substr(0, pos2);
    s.remove_prefix(pos + 1);
    // status
    pos = s.find(" ");
    if (pos == std::string_view::npos) {
        return -4;
    }
    std::string status(s.data(), pos);
    try {
        status_code_ = std::atoi(status.c_str());
    } catch (std::exception & e) {
        return -5;
    }
    s.remove_prefix(pos + 1);
    //get msg
    if (s.size() <= 0) {
        return -6;
    }
    reason_ = s;
    return pos_end + 2;
}

int32_t RtspResponse::parse_headers(const std::string_view & buf) {
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

int32_t RtspResponse::parse_header(const std::string_view & buf) {
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