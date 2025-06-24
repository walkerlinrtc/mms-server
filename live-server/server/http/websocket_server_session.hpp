#include <memory>
#include <atomic>
#include <array>

#include "server/session.hpp"
#include "base/wait_group.h"
#include "base/obj_tracker.hpp"

namespace mms {
class WebSocketPacket;
class SocketInterface;

class WebSocketServerSession : public Session, public ObjTracker<WebSocketServerSession> {
public:
    WebSocketServerSession(std::shared_ptr<SocketInterface> sock);
    virtual ~WebSocketServerSession();
    void start() override;
    void stop() override;
private:
    boost::asio::awaitable<int32_t> process_recv_buffer();
private:
    std::shared_ptr<SocketInterface> sock_;
    std::shared_ptr<WebSocketPacket> packet_;

    std::unique_ptr<char[]> recv_buf_;
    int64_t recv_buf_bytes_ = 0;
    static constexpr int64_t max_recv_buf_bytes_ = 1024*1024;

    WaitGroup wg_;
};
};