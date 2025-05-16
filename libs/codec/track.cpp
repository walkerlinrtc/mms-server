#include "track.hpp"
using namespace mms;

VideoTrack::VideoTrack(int track_id, std::shared_ptr<Codec> codec) : Track(Track::Video, track_id), codec_(codec) {

}

VideoTrack::~VideoTrack() {

}


AudioTrack::AudioTrack(int track_id, std::shared_ptr<Codec> codec) : Track(Track::Audio, track_id), codec_(codec) {

}

AudioTrack::~AudioTrack() {

}
