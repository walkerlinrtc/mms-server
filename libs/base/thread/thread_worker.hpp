
#pragma once

#include <memory>
#include <functional>
#include <thread>
#include <atomic>

#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_context.hpp>

namespace mms {
class ThreadWorker;
class ThreadWorker {
public:
    using Task = std::function<void()>;
    ThreadWorker();
    virtual ~ThreadWorker();
    // to set on which cpu to run this thread
    void set_cpu_core(int cpu_core);
    int get_cpu_core();

    template<typename F, typename ...ARGS>
    void post(F &&f, ARGS &&...args) {
        io_context_.post(std::bind(f, std::forward<ARGS>(args)...));
    }

    template<typename F, typename ...ARGS>
    void dispatch(F &&f, ARGS &&...args) {
        io_context_.dispatch(std::bind(f, std::forward<ARGS>(args)...));
    }

    class Event {
    public:
        Event(ThreadWorker *worker, const std::function<void(Event *ev)> &f);
        ~Event();
        void invoke_after(uint32_t ms);
        ThreadWorker *get_worker();
    private:
        ThreadWorker *worker_;
        std::function<void(Event *ev)> f_;
        boost::asio::deadline_timer timer_;
    };

    Event* create_event(const std::function<void(Event* ev)> &f);
    void remove_event(Event *ev);

    void start();
    void work();
    void stop();
    boost::asio::io_context & get_io_context();
private:
    int cpu_core_;
    boost::asio::io_context io_context_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
    std::jthread thread_;
    std::atomic_bool running_;
};

};