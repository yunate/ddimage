#ifndef ddimage_ddgif__h_
#define ddimage_ddgif__h_

#include "ddbase/ddmini_include.h"
#include "ddbase/ddcolor.hpp"
#include <list>
#include <functional>
namespace NSP_DD {
class ddgif
{
public:
    struct ddgif_frame
    {
        ddbitmap bitmap;
        u32 delay = 0;
    };

    ddgif() = default;
    ~ddgif();

    static ddgif* create_from_path(const std::wstring& path);
    static ddgif* create_from_buff(const ddbuff& buff);

    // 将修改commit
    bool commit();

    // 保存到内存
    bool save(ddbuff& buff) const;

    // 获取帧数
    u32 get_frame_count() const;
    bool get_frame(u32 index, ddgif_frame& bitmap);

    bool add_frame(const ddgif_frame& bitmap, u32 index);
    bool delete_frame(u32 index);
    bool replace_frame(const ddgif_frame& bitmap, u32 index);
    bool swap_frame(u32 index1, u32 index2);

private:
    bool init(const ddbuff& buff);

    u32 m_catch_index = 0;
    ddgif_frame* m_frame_catch = nullptr;

};
} // namespace NSP_DD
#endif // ddimage_ddgif__h_
