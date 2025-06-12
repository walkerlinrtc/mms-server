#include "socket_interface.hpp"
#include "socket_traffic_observer.h"
#include "spdlog/spdlog.h"

using namespace mms;
std::atomic<uint64_t> SocketInterface::socket_id_ = {0};
SocketInterface::SocketInterface(SocketInterfaceHandler *handler) : handler_(handler) {
    id_ = socket_id_++;
}

SocketInterface::~SocketInterface() {
    spdlog::debug("destroy SocketInterface");
}

Json::Value SocketInterface::to_json() {
    Json::Value v;
    v["in_bytes"] = in_bytes_;
    v["out_bytes"] = out_bytes_;
    v["remote_address"] = get_remote_address();
    v["local_address"] = get_local_address();
    return v;
}

int64_t SocketInterface::get_in_bytes() {
    return in_bytes_;
}

int64_t SocketInterface::get_out_bytes() {
    return out_bytes_;
}

void SocketInterface::add_observer(std::shared_ptr<SocketTrafficObserver> obs) {
    observers_.push_back(obs);
}

void SocketInterface::remove_observer(std::shared_ptr<SocketTrafficObserver> obs) {
    observers_.erase(std::remove(observers_.begin(), observers_.end(), obs), observers_.end());
}

void SocketInterface::notify_bytes_in(size_t bytes) {
    in_bytes_ += bytes;
    for (auto& obs : observers_) {
        obs->on_bytes_in(bytes);
    }
}

void SocketInterface::notify_bytes_out(size_t bytes) {
    out_bytes_ += bytes;
    for (auto& obs : observers_) {
        obs->on_bytes_out(bytes);
    }
}

std::string SocketInterface::get_local_address() {
    return "";
}

std::string SocketInterface::get_remote_address() {
    return "";
}

uint64_t SocketInterface::get_id() const {
    return id_;
}

void SocketInterface::set_session(std::shared_ptr<Session> session) {
    session_ = session;
}

std::shared_ptr<Session> SocketInterface::get_session() {
    return session_;
}

void SocketInterface::clear_session() {
    session_ = nullptr;
}