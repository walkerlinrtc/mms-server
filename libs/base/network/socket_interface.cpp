#include "socket_interface.hpp"

#include "spdlog/spdlog.h"

using namespace mms;
std::atomic<uint64_t> SocketInterface::socket_id_ = {0};
SocketInterface::SocketInterface(SocketInterfaceHandler *handler) : handler_(handler) {
    id_ = socket_id_++;
}

SocketInterface::~SocketInterface() {
    spdlog::info("destroy SocketInterface");
}

int64_t SocketInterface::get_in_bytes() {
    return in_bytes_;
}

int64_t SocketInterface::get_out_bytes() {
    return out_bytes_;
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