#pragma once
#include <memory>
#include "../codec.hpp"
namespace mms {
class MediaSdp;
class Payload;

class G711ACodec : public Codec {
public:
    G711ACodec() : Codec(CODEC_G711_ALOW, "G711ALOW") {

    }

    virtual ~G711ACodec() {
        
    }    
};

};