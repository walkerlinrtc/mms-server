#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>

#include "base/thread/thread_worker.hpp"

#include "stream_session.hpp"
#include "base/thread/thread_worker.hpp"
#include "config/config.h"
#include "app/app_manager.h"
#include "app/app.h"
#include "core/media_source.hpp"
#include "log/log.h"

using namespace mms;

StreamSession::StreamSession(ThreadWorker *worker) : Session(worker), wg_(worker), cleanup_timer_(worker->get_io_context()) {
}

StreamSession::~StreamSession() {

}

void StreamSession::set_session_info(const std::string & domain, const std::string & app_name, const std::string & stream_name) {
    domain_name_ = domain;
    app_name_ = app_name;
    stream_name_ = stream_name;
    session_name_ = domain_name_ + "/" + app_name_ + "/" + stream_name_;
}

void StreamSession::set_session_info(const std::string_view & domain, const std::string_view & app_name, const std::string_view & stream_name) {
    domain_name_ = domain;
    app_name_ = app_name;
    stream_name_ = stream_name;
    session_name_ = domain_name_ + "/" + app_name_ + "/" + stream_name_;
}

bool StreamSession::find_and_set_app(const std::string & domain, const std::string & app_name, bool is_publish) {
    auto app = AppManager::get_instance().get_app(domain, app_name);
    if (!app) {
        return false;
    }
    
    if (app->is_publish_app() != is_publish) {
        CORE_WARN("app: {} is not publish app", app_name);
        return false;
    }

    is_publish_ = is_publish;
    app_ = app;
    return true;
}

void StreamSession::start_delayed_source_check_and_delete(uint32_t delay_sec, std::shared_ptr<MediaSource> media_source) {
    auto self(shared_from_this());
    if (waiting_cleanup_) {
        return;
    }

    waiting_cleanup_ = true;
    wg_.add(1);
    boost::asio::co_spawn(worker_->get_io_context(), [this, self, delay_sec, media_source]()->boost::asio::awaitable<void> {
        cleanup_timer_.expires_from_now(std::chrono::milliseconds(delay_sec*1000));
        boost::system::error_code ec;
        co_await cleanup_timer_.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) {
            co_return;//被cancel了，可能是重新绑定了，不需要检查le
        }

        auto session = media_source->get_session();
        if (!session) {// session已经解除绑定，那么可以删除
            media_source->close();
        } else if (session != self) {
            CORE_DEBUG("source:{} is changed session", get_session_name());
        }
        co_return;
    }, [this, self](std::exception_ptr exp) {
        (void)exp;
        waiting_cleanup_ = false;
        wg_.done();
    });
}

// void StreamSession::stop_source_delay_check_and_delete() {
//     auto self(shared_from_this());
//     if (!waiting_cleanup_) {
//         co_return;
//     }

//     boost::asio::co_spawn(worker_->get_io_context(), [this, self, media_source]()->boost::asio::awaitable<void> {
//         cleanup_timer_.cancel();
//         co_await wg_.wait();
//     }, boost::asio::detached);
// }