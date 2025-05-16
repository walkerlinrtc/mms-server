#pragma once
#include <memory>

#include "server/tcp/tcp_server.hpp"
#include "base/thread/thread_pool.hpp"

namespace mms {
class RtmpServer : public TcpServer, public SocketInterfaceHandler {
public:
    RtmpServer(ThreadWorker *w);
    virtual ~RtmpServer();
    
    bool start(uint16_t port = 1935);
    void stop();
private:
    void on_socket_open(std::shared_ptr<SocketInterface> tcp_socket) override;
    void on_socket_close(std::shared_ptr<SocketInterface> tcp_socket) override;
};
};