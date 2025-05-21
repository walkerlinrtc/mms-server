#include <boost/asio/awaitable.hpp>
#include <memory>

namespace mms {
class MediaSource;
class StreamSession;
// define a common interface for source finder obj
class MediaSourceFinder {
public:
    MediaSourceFinder() = default;
    virtual ~MediaSourceFinder() = default;
public:
    virtual boost::asio::awaitable<std::shared_ptr<MediaSource>> find_media_source(std::shared_ptr<StreamSession> session) {
        (void)session;
        co_return nullptr;
    }
};

};