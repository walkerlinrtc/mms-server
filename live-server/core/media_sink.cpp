/*
 * @Author: jbl19860422
 * @Date: 2023-12-22 22:33:04
 * @LastEditTime: 2023-12-27 21:14:11
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\core\media_sink.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>

#include "media_sink.hpp"
#include "media_event.hpp"

#include "base/thread/thread_worker.hpp"
#include "spdlog/spdlog.h"
using namespace mms;

MediaSink::MediaSink(ThreadWorker *worker) : worker_(worker) {
    
}

MediaSink::~MediaSink() {
    spdlog::info("destroy MediaSink");
}

void MediaSink::close() {
    if (close_cb_) {
        close_cb_();
    }
}

void MediaSink::on_close(const std::function<void()> & cb) {
    close_cb_ = cb;
}

ThreadWorker *MediaSink::get_worker() {
    return worker_;
}

void MediaSink::set_source(std::shared_ptr<MediaSource> source) {
    source_ = source;
}

std::shared_ptr<MediaSource> MediaSink::get_source() {
    return source_;
}

void MediaSink::set_event_cb(const std::function<void(const MediaEvent & ev)> & ev_cb) {
    ev_cb_ = ev_cb;
}

void MediaSink::recv_event(const MediaEvent & ev) {
    auto self(shared_from_this());
    // 在自己的worker里面进行处理
    boost::asio::co_spawn(worker_->get_io_context(), [ev, this, self]()->boost::asio::awaitable<void> {
        if (ev_cb_) {
            ev_cb_(ev);
        }
        co_return;
    }, boost::asio::detached);
}

LazyMediaSink::LazyMediaSink(ThreadWorker *worker) : MediaSink(worker),  wg_(worker) {

}

LazyMediaSink::~LazyMediaSink() {
}

void LazyMediaSink::wakeup() {
    if (working_ || closing_) {
        return;
    }
    
    working_ = true;
    auto self(shared_from_this());
    boost::asio::co_spawn(worker_->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        co_await do_work();
        co_return;
    }, [this, self](std::exception_ptr exp) {
        (void)(exp);
        working_ = false;
        if (closing_) {
            wg_.done();
        }
    });
}

void LazyMediaSink::close() {
    closing_ = true;
    if (working_) {
        wg_.add(1);
        auto self(shared_from_this());
        // 确保wakeup结束了，自己才释放
        boost::asio::co_spawn(worker_->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
            co_await wg_.wait();
            MediaSink::close();
        }, boost::asio::detached);
    }
}

