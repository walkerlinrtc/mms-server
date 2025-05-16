#pragma once
#include <string>
#include <memory>

namespace mms {
class Codec;
class Track {
public:
    enum TrackType {
        Video,
        Audio,
        Subtitle
    };

    Track(TrackType type, int track_id)
        : type_(type), trace_id_(track_id)
    {

    }

    virtual ~Track() {

    }

    TrackType getType() const { 
        return type_; 
    }

    int getTrackID() const { 
        return trace_id_;
    }

protected:
    TrackType type_;
    int trace_id_ = 0;      // 轨道ID
};

class VideoTrack : public Track {
public:
    VideoTrack(int track_id, std::shared_ptr<Codec> codec);
    virtual ~VideoTrack();

    std::shared_ptr<Codec> getCodec() const { 
        return codec_;
    }
protected:
    std::shared_ptr<Codec> codec_;
};

class AudioTrack : public Track {
public:
    AudioTrack(int track_id, std::shared_ptr<Codec> codec);
    virtual ~AudioTrack();

    std::shared_ptr<Codec> getCodec() const { 
        return codec_;
    }
protected:
    std::shared_ptr<Codec> codec_;
};

};