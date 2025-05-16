#include <vector>
#include <stdint.h>
#include <stack>
#include "base/chunk.h"
#include "box.h"
#include "base/chunk_manager.h"

namespace mms {
class MoovBuilder;
class FtypBuilder;

class Mp4Builder {
public:
    Mp4Builder();
    virtual ~Mp4Builder();
    ChunkManager & get_chunk_manager() {
        return chunk_manager_;
    }
private:
    using SizePos = struct {
        std::shared_ptr<Chunk> size_chunk_ptr_;
        size_t size_chunk_offset_ = 0;
    };

    std::stack<SizePos> box_stack_;  // 用于嵌套 Box 的大小回填
public:
    ChunkManager chunk_manager_;
public:
    // 开始一个 Box
    void begin_box(Box::Type type);
    // 结束当前 Box，回填实际大小
    void end_box();
    // 添加 ftyp Box
    FtypBuilder add_ftyp();
    MoovBuilder add_moov();

};
};