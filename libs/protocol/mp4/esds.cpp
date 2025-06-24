#include "esds.h"
#include "base/net_buffer.h"
#include "spdlog/spdlog.h"
using namespace mms;

Mp4BaseDescriptor::Mp4BaseDescriptor() {

}

Mp4BaseDescriptor::~Mp4BaseDescriptor() {

}

int64_t Mp4BaseDescriptor::size() {
    int64_t total_bytes = 1;
    uint32_t len = vlen_;
    do {
        total_bytes += 1;
        len >>= 7;
    } while (len > 0);
    return total_bytes;
}

int64_t Mp4BaseDescriptor::encode(NetBuffer & buf) {
    auto start = buf.pos();
    buf.write_1bytes(tag_);
    uint32_t len = vlen_;
    while (len > 0) {
        if (len > 0x7f) {
            buf.write_1bytes((len&0x7f)|0x80);
        } else {
            buf.write_1bytes((len&0x7f)&(~0x80));
        }
        len = len >> 7;
    }
    return buf.pos() - start;
}

int64_t Mp4BaseDescriptor::decode(NetBuffer & buf) {
    auto start = buf.pos();
    tag_ = (Mp4ESTagEs)buf.read_1bytes();
    uint8_t v = 0x80;
    uint32_t len = 0;
    while((v & 0x80) == 0x80) {
        len = (len << 7) | (v&0x7f);
        v = buf.read_1bytes();
    }
    vlen_ = len;
    return buf.pos() - start;
}

Mp4SLConfigDescriptor::Mp4SLConfigDescriptor() {
    tag_ = Mp4ESTagESSLConfigDescrTag;
    predefined_ = 2;
}

Mp4SLConfigDescriptor::~Mp4SLConfigDescriptor() {
    
}

int64_t Mp4SLConfigDescriptor::size() {
    vlen_ = 1;
    int64_t total_bytes = Mp4BaseDescriptor::size();
    total_bytes += 1;
    return total_bytes;
}

int64_t Mp4SLConfigDescriptor::encode(NetBuffer & buf) {
    auto start = buf.pos();
    Mp4BaseDescriptor::encode(buf);
    buf.write_1bytes(predefined_);
    return buf.pos() - start;
}

int64_t Mp4SLConfigDescriptor::decode(NetBuffer & buf) {
    auto start = buf.pos();
    Mp4BaseDescriptor::decode(buf);
    predefined_ = buf.read_1bytes();
    return buf.pos() - start;
}

Mp4DecoderSpecificInfo::Mp4DecoderSpecificInfo() {
    tag_ = Mp4ESTagESDecSpecificInfoTag;
}

Mp4DecoderSpecificInfo::~Mp4DecoderSpecificInfo() {

}

int64_t Mp4DecoderSpecificInfo::size() {
    vlen_ = asc_.size();
    int64_t total_bytes = Mp4BaseDescriptor::size();
    total_bytes += asc_.size();
    return total_bytes;
}

int64_t Mp4DecoderSpecificInfo::encode(NetBuffer & buf) {
    auto start = buf.pos();
    Mp4BaseDescriptor::encode(buf);
    buf.write_string(asc_);
    return buf.pos() - start;
}

int64_t Mp4DecoderSpecificInfo::decode(NetBuffer & buf) {
    auto start = buf.pos();
    Mp4BaseDescriptor::decode(buf);
    asc_ = buf.read_string(vlen_);
    return buf.pos() - start;
}

Mp4DecoderConfigDescriptor::Mp4DecoderConfigDescriptor() {
    tag_ = Mp4ESTagESDecoderConfigDescrTag;
    object_type_indication_ = Mp4ObjectTypeForbidden;
    stream_type_ = Mp4StreamTypeForbidden;
    reserved_ = 1;
}

Mp4DecoderConfigDescriptor::~Mp4DecoderConfigDescriptor() {

}

int64_t Mp4DecoderConfigDescriptor::size() {
    vlen_ = 13;
    if (dec_specific_info_) {
        vlen_ += dec_specific_info_->size();
    }
    int64_t total_bytes = Mp4BaseDescriptor::size();
    return total_bytes + vlen_;
}

int64_t Mp4DecoderConfigDescriptor::encode(NetBuffer & buf) {
    auto start = buf.pos();
    Mp4BaseDescriptor::encode(buf);
    buf.write_1bytes(object_type_indication_);
    uint8_t v = reserved_;
    v |= (upstream_&0x01)<<1;
    v |= uint8_t(stream_type_&0x3f)<<2;
    buf.write_1bytes(v);
    buf.write_3bytes(buffer_size_db_);
    buf.write_4bytes(max_bitrate_);
    buf.write_4bytes(avg_bitrate_);

    if (dec_specific_info_) {
        dec_specific_info_->encode(buf);
    }
    return buf.pos() - start;
}

int64_t Mp4DecoderConfigDescriptor::decode(NetBuffer & buf) {
    auto start = buf.pos();
    Mp4BaseDescriptor::decode(buf);
    object_type_indication_ = (Mp4ObjectType)buf.read_1bytes();
    uint8_t v = buf.read_1bytes();
    upstream_ = (v>>1)&0x01;
    stream_type_ = Mp4StreamType((v>>2)&0x3f);
    buffer_size_db_ = buf.read_3bytes();
    max_bitrate_ = buf.read_4bytes();
    avg_bitrate_ = buf.read_4bytes();
    if (vlen_ > 13) {
        dec_specific_info_ = std::make_shared<Mp4DecoderSpecificInfo>();
        dec_specific_info_->decode(buf);
    }
    return buf.pos() - start;
}

Mp4ES_Descriptor::Mp4ES_Descriptor() {
    tag_ = Mp4ESTagESDescrTag;
    streamPriority_ = streamDependenceFlag_ = URL_Flag_ = OCRstreamFlag_ = 0;
}

Mp4ES_Descriptor::~Mp4ES_Descriptor() {

}

int64_t Mp4ES_Descriptor::size() {
    vlen_ = 2 + 1;
    vlen_ += streamDependenceFlag_? 2:0;
    if (URL_Flag_) {
        vlen_ += 1 + URLstring_.size();
    }
    vlen_ += OCRstreamFlag_? 2:0;
    vlen_ += decConfigDescr_.size() + slConfigDescr_.size();

    int64_t total_bytes = Mp4BaseDescriptor::size();
    total_bytes += vlen_;
    return total_bytes;
}

int64_t Mp4ES_Descriptor::encode(NetBuffer & buf) {
    auto start = buf.pos();
    Mp4BaseDescriptor::encode(buf);

    buf.write_2bytes(ES_ID_);
    uint8_t v = streamPriority_ & 0x1f;
    v |= (streamDependenceFlag_ & 0x01) << 7;
    v |= (URL_Flag_ & 0x01) << 6;
    v |= (OCRstreamFlag_ & 0x01) << 5;
    buf.write_1bytes(v);
    
    if (streamDependenceFlag_) {
        buf.write_2bytes(dependsOn_ES_ID_);
    }
    
    if (URL_Flag_ && !URLstring_.empty()) {
        buf.write_1bytes(URLstring_.size());
        buf.write_string(URLstring_);
    }
    
    if (OCRstreamFlag_) {
        buf.write_2bytes(OCR_ES_Id_);
    }
    
    decConfigDescr_.encode(buf);
    slConfigDescr_.encode(buf);
    return buf.pos() - start;
}

int64_t Mp4ES_Descriptor::decode(NetBuffer & buf) {
    auto start = buf.pos();
    ES_ID_ = buf.read_2bytes();
    
    uint8_t v = buf.read_1bytes();
    streamPriority_ = v & 0x1f;
    streamDependenceFlag_ = (v >> 7) & 0x01;
    URL_Flag_ = (v >> 6) & 0x01;
    OCRstreamFlag_ = (v >> 5) & 0x01;
    
    if (streamDependenceFlag_) {
        dependsOn_ES_ID_ = buf.read_2bytes();
    }
    
    if (URL_Flag_) {
        uint8_t URLlength = buf.read_1bytes();
        URLstring_ = buf.read_string(URLlength);
    }
    
    if (OCRstreamFlag_) {
        OCR_ES_Id_ = buf.read_2bytes();
    }
    
    decConfigDescr_.decode(buf);
    slConfigDescr_.decode(buf);
    return buf.pos() - start;
}

EsdsBox::EsdsBox() : FullBox(BOX_TYPE_ESDS, 0, 0) {

}

EsdsBox::~EsdsBox() {

}

int64_t EsdsBox::size() {
    int64_t total_bytes = FullBox::size();
    total_bytes += es_.size();
    return total_bytes;
}

int64_t EsdsBox::encode(NetBuffer & buf) {
    update_size();
    auto start = buf.pos();
    FullBox::encode(buf);
    es_.encode(buf);
    return buf.pos() - start;
}

int64_t EsdsBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    FullBox::decode(buf);
    es_.decode(buf);
    return buf.pos() - start;
}


