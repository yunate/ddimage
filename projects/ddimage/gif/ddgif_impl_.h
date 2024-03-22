#ifndef ddimage_gif_ddgif_impl__h_
#define ddimage_gif_ddgif_impl__h_

#include "ddimage/ddimage.h"
#include "ddimage/gif/ddgif_info_.h"
#include "ddbase/ddmini_include.h"
#include "ddbase/ddcolor.h"

#include <functional>
namespace NSP_DD {
class ddgif_string_table;

class ddgif_impl
{
public:
    DDDEFAULT_CONSTRUCT_ALL(ddgif_impl);

    static ddgif_impl* create_from_path(const std::wstring& path);
    static ddgif_impl* create_from_buff(const ddbuff& buff);

    u32 get_frame_count();

    // index:[0, count)
    bool get_frame(u32 index, ddgif_frame_info& info);

    // index:[0, count)
    void set_time_delay(u32 index, s32 ms);

    // 设置图片质量, 当图片修改时候重新量化时的质量
    // quality越大, 失真率越小,但体积越大, 反正亦然
    // 不会影响已经编码的frame, 只会影响后续编码(insert_frame等)的frame
    // set_quality[2, 8]
    void set_quality(u8 quality);

    // 在index前插入. index:[0, count], -1 代表往尾部插入
    // 从尾部插入的效率更高,因为它需要解码的次数更少
    // 1. get pre frame
    // 2. get next frame
    // 3. encode current and insert
    // 4. get current frame
    // 5. encode next frame and replace it
    bool insert_frame(const ddgif_frame_info& image, s32 index);

    // 删除index:[0, count)位置的frame
    // 从尾部删除的效率极高,因为它只是简单的从image desc中移除
    // 1. get pre frame
    // 2. get next frame
    // 3. encode next frame and replace it
    bool delete_frame(u32 index);

    // 将index:[0, count)位置的frame替换为新的image
    // 1. get pre frame
    // 2. encode current frame and replace it
    // 3. encode next frame and replace it
    bool replace_frame(const ddgif_frame_info& image, u32 index);

    bool save(ddbuff& buff);
    bool save(const std::wstring& path);
private:
    bool init(const ddbuff& buff);
    void decode(ddgif_image_desc* desc, ddgif_string_table* string_table, bool first_frame);
    ddgif_frame_info* decode(u32 index);
    ddgif_image_desc* create_image_desc(const ddbitmap* bitmap, s32 delay_time, const ddbitmap* pre_bitmap);
    void rebuild_logic_screen_desc();
    ddgif_info m_gif_info;
    u16 m_palette_size = 256;

    // catch
    u32 get_catch_index();
    ddgif_image_desc* m_catch_point = nullptr;
    ddgif_frame_info m_catch_info;
};
} // namespace NSP_DD
#endif // ddimage_gif_ddgif_impl__h_
