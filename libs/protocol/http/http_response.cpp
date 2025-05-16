#include <sstream>
#include "http_response.hpp"
#include "base/network/socket_interface.hpp"
#include "spdlog/spdlog.h"

using namespace mms;
HttpResponse::HttpResponse(std::shared_ptr<SocketInterface> conn, size_t buffer_size) : conn_(conn) {
    recv_buf_ = std::make_unique<char[]>(buffer_size);
    buffer_size_ = buffer_size;
}

HttpResponse::~HttpResponse() {
}

std::shared_ptr<SocketInterface> HttpResponse::get_conn() {
    return conn_;
}

void HttpResponse::add_header(const std::string & k, const std::string & v) {
    headers_[k] = v;
}

boost::asio::awaitable<bool> HttpResponse::write_header(int code, const std::string & reason) {
    std::ostringstream ss;
    ss  << "HTTP/1.1 " << code << " " << reason << "\r\n";
    for(auto & h : headers_) {
        ss << h.first << ": " << h.second << "\r\n";
    }
    ss << "\r\n";
    std::string header = ss.str();
    if (!(co_await conn_->send((const uint8_t*)header.c_str(), header.size()))) {
        co_return false;
    }
    co_return true;
}

boost::asio::awaitable<bool> HttpResponse::write_data(const uint8_t *data, size_t len) {
    bool ret = true;
    ret = co_await conn_->send(data, len);
    co_return ret;
}

boost::asio::awaitable<bool> HttpResponse::write_data(const std::string_view & buf) {
    bool ret = true;
    ret = co_await conn_->send((const uint8_t*)buf.data(), buf.size());
    co_return ret;
}

boost::asio::awaitable<bool> HttpResponse::write_data(std::vector<boost::asio::const_buffer> & bufs) {
    bool ret = true;
    ret = co_await conn_->send(bufs);
    co_return ret;
}


const std::string & HttpResponse::get_version() const {
    return version_;
}

int32_t HttpResponse::get_status_code() const {
    return status_code_;
}

const std::string & HttpResponse::get_msg() const {
    return msg_;
}

boost::asio::awaitable<bool> HttpResponse::recv_http_header() {
    int32_t consumed = 0;
    while (1) {
        if (buffer_size_ - recv_buf_size_ <= 0) {
            co_return false;
        }

        auto recv_size = co_await conn_->recv_some((uint8_t*)recv_buf_.get() + recv_buf_size_, buffer_size_ - recv_buf_size_);
        if (recv_size < 0) {
            co_return false;
        }
        
        recv_buf_size_ += recv_size;
        std::string_view buf(recv_buf_.get(), recv_buf_size_);
        consumed = parse_http_header(buf);
        if (consumed < 0) {
            co_return false;
        }

        if (consumed > 0) {
            recv_buf_size_ -= consumed;
            memmove(recv_buf_.get(), recv_buf_.get() + consumed, recv_buf_size_);
            break;
        }
    }
    co_return true;
}

int32_t HttpResponse::parse_http_header(const std::string_view & buf) {
    auto pos_end = buf.find(HTTP_HEADER_DIVIDER);
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

int32_t HttpResponse::parse_status_line(const std::string_view & buf) {
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
    msg_ = s;
    return pos_end + 2;
}

int32_t HttpResponse::parse_headers(const std::string_view & buf) {
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

int32_t HttpResponse::parse_header(const std::string_view & buf) {
    auto pos_end = buf.find(HTTP_CRLF);
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

boost::asio::awaitable<void> HttpResponse::cycle_recv_body(const std::function<boost::asio::awaitable<int32_t>(const std::string_view &)> & cb) {
    auto it_content_len = headers_.find("Content-Length");
    auto it_transfer_encoding = headers_.find("Transfer-Encoding");
    int32_t content_len = -1;
    bool is_chunked = false;
    if (it_content_len != headers_.end()) {
        try {
            content_len = std::atoi(it_content_len->second.c_str());
        } catch (std::exception & e) {
            co_return;
        }

        if (content_len < 0) {
            co_return;//illegal
        }
    } else if (it_transfer_encoding != headers_.end()) {
        if (it_transfer_encoding->second == "chunked") {
            is_chunked = true;
        }
    }
    //todo:考虑chunk的情况
    int32_t consumed = 0;
    if (content_len > 0) {
        while (true) {
            while (recv_buf_size_ < content_len) {
                if (recv_buf_size_ >= buffer_size_) {
                    co_return;
                }
                
                if (buffer_size_ - recv_buf_size_ == 0) {
                    spdlog::error("no space");
                    conn_->close();
                    co_return;
                }
                
                auto recv_size = co_await conn_->recv_some((uint8_t*)recv_buf_.get() + recv_buf_size_, buffer_size_ - recv_buf_size_);
                if (recv_size < 0) {
                    co_return;
                }
                recv_buf_size_ += recv_size;
            }

            int32_t total_consumed = 0;
            while (total_consumed < recv_buf_size_) {
                consumed = co_await cb(std::string_view(recv_buf_.get() + total_consumed, recv_buf_size_ - total_consumed));
                if (consumed < 0) {
                    co_return;
                } else if (consumed == 0) {
                    break;
                }
                total_consumed += consumed;
            }

            if (total_consumed > 0) {
                recv_buf_size_ -= total_consumed;
                memmove(recv_buf_.get(), recv_buf_.get() + total_consumed, recv_buf_size_);
            }
            co_return;
        }
        co_return;
    } else {
        if (is_chunked) {
            if (!recv_chunk_buf_) {
                recv_chunk_buf_ = std::make_unique<char[]>(buffer_size_);
            }
            
            while (1) {
                int32_t total_consumed = 0;
                bool last_chunk = false;
                while (recv_buf_size_ > 0 && total_consumed < recv_buf_size_) {
                    consumed = extract_chunks(std::string_view(recv_buf_.get() + total_consumed, recv_buf_size_ - total_consumed), last_chunk);
                    if (consumed < 0) {
                        co_return;
                    } else if (consumed == 0) {
                        break;
                    }
                    total_consumed += consumed;
                } 

                if (total_consumed > 0) {
                    recv_buf_size_ -= total_consumed;
                    memmove(recv_buf_.get(), recv_buf_.get() + total_consumed, recv_buf_size_);
                }

                int total_chunk_consumed = 0;
                while (recv_chunk_buf_size_ > 0 && total_chunk_consumed < recv_chunk_buf_size_) {
                    consumed = co_await cb(std::string_view(recv_chunk_buf_.get() + total_chunk_consumed, recv_chunk_buf_size_ - total_chunk_consumed));
                    if (consumed < 0) {
                        co_return;
                    } else if (consumed == 0) {
                        break;
                    }
                    total_chunk_consumed += consumed;
                }

                if (total_chunk_consumed > 0) {
                    recv_chunk_buf_size_ -= total_chunk_consumed;
                    memmove(recv_chunk_buf_.get(), recv_chunk_buf_.get() + total_chunk_consumed, recv_chunk_buf_size_);
                }
                
                if (!last_chunk) {
                    if (buffer_size_ - recv_buf_size_ == 0) {
                        spdlog::error("no space");
                        conn_->close();
                        co_return;
                    }

                    auto recv_size = co_await conn_->recv_some((uint8_t*)recv_buf_.get() + recv_buf_size_, buffer_size_ - recv_buf_size_);
                    if (recv_size < 0) {
                        co_return;
                    }
                    recv_buf_size_ += recv_size;
                } else {
                    co_return;
                }
            }
        } else {
            while (1) {
                int32_t total_consumed = 0;
                while (recv_buf_size_ > 0 && total_consumed < recv_buf_size_) {
                    consumed = co_await cb(std::string_view(recv_buf_.get() + total_consumed, recv_buf_size_ - total_consumed));
                    if (consumed < 0) {
                        co_return;
                    } else if (consumed == 0) {
                        break;
                    }
                    total_consumed += consumed;
                }
                
                if (total_consumed > 0) {
                    recv_buf_size_ -= total_consumed;
                    memmove(recv_buf_.get(), recv_buf_.get() + total_consumed, recv_buf_size_);
                }
                
                if (buffer_size_ - recv_buf_size_ <= 0) {
                    spdlog::error("no space");
                    conn_->close();
                    co_return;
                }

                auto recv_size = co_await conn_->recv_some((uint8_t*)recv_buf_.get() + recv_buf_size_, buffer_size_ - recv_buf_size_);
                if (recv_size < 0) {
                    co_return;
                }
                recv_buf_size_ += recv_size;
            }
        }
    }

    co_return;
}

int32_t HttpResponse::extract_chunks(const std::string_view & buf, bool & last_chunk) {
    int32_t total_consumed = 0;
    std::string_view buf_in = buf;
    while (buf_in.size() > 2) {
        auto pos_crlf = buf_in.find(HTTP_CRLF);
        if (pos_crlf == std::string_view::npos) {
            return total_consumed;
        }

        size_t chunk_bytes = 0;
        try {
            chunk_bytes = std::stoi(std::string(buf_in.data(), pos_crlf), 0, 16);
        } catch (std::exception & e) {
            return -1;
        }

        std::string_view buf_tmp(buf_in);
        buf_tmp.remove_prefix(pos_crlf + 2);
        if (buf_tmp.size() < chunk_bytes + 2) {
            return total_consumed;
        }

        auto pos_chunk_data_crlf = chunk_bytes;
        if (buf_tmp[pos_chunk_data_crlf] != '\r' || buf_tmp[pos_chunk_data_crlf + 1] != '\n') {
            return -2;
        }

        if (chunk_bytes > 0) {
            memcpy(recv_chunk_buf_.get() + recv_chunk_buf_size_, buf_tmp.data(), chunk_bytes);
            recv_chunk_buf_size_ += chunk_bytes;
        } else if (chunk_bytes == 0) {
            last_chunk = true;
        }
    
        total_consumed += (pos_crlf + 2 + chunk_bytes + 2);
        buf_in.remove_prefix(pos_crlf + 2 + chunk_bytes + 2);
    }
    return total_consumed;
}

void HttpResponse::close() {
    conn_->close();
    return;
}

ThreadWorker *HttpResponse::get_worker() {
    return conn_->get_worker();
}

const std::string & HttpResponse::get_header(const std::string & key) {
    auto it = headers_.find(key);
    if (it == headers_.end()) {
        static std::string emp_str;
        return emp_str;
    }
    return it->second;
}