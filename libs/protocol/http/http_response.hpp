#pragma once
#include <unordered_map>
#include <string>
#include <string_view>
#include <memory>
#include <functional>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>

#include "http_define.h"

namespace mms {
class ThreadWorker;
class SocketInterface;
class HttpServerSession;

class HttpResponse {
public:
    // 服务器模式
    HttpResponse(std::shared_ptr<SocketInterface> conn, std::weak_ptr<HttpServerSession> session, size_t buffer_size = 8192);
    // 客户端模式
    HttpResponse(std::shared_ptr<SocketInterface> conn, size_t buffer_size = 8192);
    virtual ~HttpResponse();
    std::shared_ptr<SocketInterface> get_conn();

    void add_header(const std::string & k, const std::string & v);
    boost::asio::awaitable<bool> write_header(int code, const std::string & reason);
    boost::asio::awaitable<bool> write_data(const uint8_t *data, size_t len);
    boost::asio::awaitable<bool> write_data(const std::string_view & buf);
    boost::asio::awaitable<bool> write_data(std::vector<boost::asio::const_buffer> & bufs);
    const std::string & get_version() const;
    int32_t get_status_code() const;
    const std::string & get_msg() const;
    boost::asio::awaitable<bool> recv_http_header();
private:
    int32_t parse_http_header(const std::string_view & buf);
    int32_t parse_status_line(const std::string_view & buf);
    int32_t parse_headers(const std::string_view & buf);
    int32_t parse_header(const std::string_view & buf);
public:
    boost::asio::awaitable<void> cycle_recv_body(const std::function<boost::asio::awaitable<int32_t>(const std::string_view &)> & cb);
private:
    int32_t extract_chunks(const std::string_view & buf, bool & last_chunk);
public:
    void close(bool keep_alive = true);
    ThreadWorker *get_worker();
    const std::string & get_header(const std::string & key);
private:
    std::string version_;
    int32_t status_code_ = 200;
    std::string msg_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
    int32_t content_len_ = -1;

    std::shared_ptr<SocketInterface> conn_;
    std::unique_ptr<char[]> recv_buf_;
    int32_t recv_buf_size_ = 0;
    std::unique_ptr<char[]> recv_chunk_buf_;
    int32_t recv_chunk_buf_size_ = 0;
    int32_t buffer_size_ = 8192;
    std::weak_ptr<HttpServerSession> session_;
};
};