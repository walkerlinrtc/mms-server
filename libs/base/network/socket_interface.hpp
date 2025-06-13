/*
 * @Author: jbl19860422
 * @Date: 2023-12-06 20:52:18
 * @LastEditTime: 2023-12-06 21:36:10
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\base\network\socket_interface.hpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once 
#include <memory>
#include <atomic>
#include <vector>

#include <boost/asio/buffer.hpp>
#include <boost/asio/awaitable.hpp>

#include "json/json.h"
namespace mms {
class ThreadWorker;
class Session;
class SocketInterface;
class SocketTrafficObserver;

class SocketInterfaceHandler {
public:
    virtual void on_socket_open(std::shared_ptr<SocketInterface> sock) = 0;
    virtual void on_socket_close(std::shared_ptr<SocketInterface> sock) = 0;
};

class SocketInterface : public std::enable_shared_from_this<SocketInterface> {
public:
    SocketInterface(SocketInterfaceHandler * handler);
    virtual ~SocketInterface();
    virtual boost::asio::awaitable<bool> connect(const std::string & ip, uint16_t port, int32_t timeout_ms = 0) = 0;
    virtual boost::asio::awaitable<bool> send(const uint8_t *data, size_t len, int32_t timeout_ms = 0) = 0;
    virtual boost::asio::awaitable<bool> send(std::vector<boost::asio::const_buffer> & bufs, int32_t timeout_ms = 0) = 0;
    virtual boost::asio::awaitable<bool> recv(uint8_t *data, size_t len, int32_t timeout_ms = 0) = 0;
    virtual boost::asio::awaitable<int32_t> recv_some(uint8_t *data, size_t len, int32_t timeout_ms = 0) = 0;
    virtual std::string get_local_address();
    virtual std::string get_remote_address();
    virtual void open() = 0;
    virtual void close() = 0;
    virtual ThreadWorker *get_worker() = 0;
    uint64_t get_id() const;

    int64_t get_in_bytes();
    int64_t get_out_bytes();

    void add_observer(std::shared_ptr<SocketTrafficObserver> obs);
    void remove_observer(std::shared_ptr<SocketTrafficObserver> obs);
    void notify_bytes_in(size_t bytes);
    void notify_bytes_out(size_t bytes);

    virtual Json::Value to_json();

    void set_session(std::shared_ptr<Session> session);
    std::shared_ptr<Session> get_session();
    void clear_session();
public:
    static std::atomic<uint64_t> socket_id_;
protected:
    SocketInterfaceHandler *handler_ = nullptr;
    std::shared_ptr<Session> session_;
    std::atomic_flag closed_ = ATOMIC_FLAG_INIT;
    int64_t in_bytes_ = 0;
    int64_t out_bytes_ = 0;
    uint64_t id_;
protected:
    std::vector<std::shared_ptr<SocketTrafficObserver>> observers_;
};
};