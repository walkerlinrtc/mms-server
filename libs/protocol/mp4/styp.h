#pragma once
#include "full_box.h"
#include "ftyp.h"
namespace mms {
// 8.16.2 Segment Type Box (styp)
// ISO_IEC_14496-12-base-format-2012.pdf, page 105
// If segments are stored in separate files (e.g. on a standard HTTP server) it is recommended that these 
// 'segment files' contain a segment-type box, which must be first if present, to enable identification of those files, 
// And declaration of the specifications with which they are compliant.
class StypBox : public FtypBox {
public:
    StypBox();
    virtual ~StypBox();
};
};