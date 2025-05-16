#pragma once
#include <memory>
#include "../codec.hpp"
namespace mms {
class MediaSdp;
class Payload;

class G711UCodec : public Codec {
public:
    G711UCodec() : Codec(CODEC_G711_ULOW, "G711ULOW") {

    }

    virtual ~G711UCodec() {
        
    }    
};

};