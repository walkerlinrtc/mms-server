#include "udp_server.hpp"

#include "base/network/udp_socket.hpp"
#include "base/thread/thread_pool.hpp"
#include "spdlog/spdlog.h"

using namespace mms;

UdpServer::UdpServer(const std::vector<ThreadWorker*>& workers) : workers_(workers) {
    udp_socks_.resize(workers.size());
    recv_bufs_.resize(workers.size());
}

UdpServer::~UdpServer() {
    for (auto udp_sock : udp_socks_) {
        delete udp_sock;
        udp_sock = nullptr;
    }
    udp_socks_.clear();
}

bool UdpServer::start_listen(const std::string& ip, uint16_t port) {
    if (workers_.size() <= 0) {
        return false;
    }

    port_ = port;
    for (size_t i = 0; i < workers_.size(); i++) {
        ThreadWorker* worker = workers_[i];
        boost::asio::co_spawn(
            worker->get_io_context(),
            [this, ip, port, worker, i]() -> boost::asio::awaitable<bool> {
                running_ = true;
                boost::system::error_code ec;
                auto sock = std::unique_ptr<boost::asio::ip::udp::socket>(
                    new boost::asio::ip::udp::socket(worker->get_io_context()));
                boost::asio::ip::udp::endpoint local_endpoint(boost::asio::ip::make_address(ip), port);
                sock->open(boost::asio::ip::udp::v4());
                int opt = 1;
                setsockopt(sock->native_handle(), SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
                sock->bind(local_endpoint, ec);
                if (ec) {
                    spdlog::error("bind udp socket failed");
                } else {
                    spdlog::info("bind udp socket succeed");
                }

                if (!sock->is_open()) {
                    spdlog::error("listen udp port:{} failed", port);
                    co_return false;
                }

                udp_socks_[i] = new UdpSocket(this, std::move(sock));
                while (running_) {
                    boost::asio::ip::udp::endpoint remote_endpoint;
                    size_t len = co_await udp_socks_[i]->recv_from(recv_bufs_[i].data(), MAX_UDP_RECV_BUF,
                                                                   remote_endpoint);
                    if (!ec) {
                        std::unique_ptr<uint8_t[]> recv_data = std::unique_ptr<uint8_t[]>(new uint8_t[len]);
                        memcpy(recv_data.get(), recv_bufs_[i].data(), len);
                        co_await on_udp_socket_recv(udp_socks_[i], std::move(recv_data), len,
                                                    remote_endpoint);
                    }
                }
                co_return true;
            },
            [this](std::exception_ptr exp, bool val) {
                ((void)exp);
                ((void)val);
            });
    }

    return true;
}

void UdpServer::stop_listen() { running_ = false; }