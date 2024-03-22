#ifndef ddimage_ddimage_h_
#define ddimage_ddimage_h_

#include "ddbase/ddmini_include.h"
#include "ddbase/ddcolor.h"
#include "ddbase/ddbitmap.h"
#include <functional> 

namespace NSP_DD {
// 设置log proc, 默认debug: std::cout, release 无
 void ddimage_set_log_proc(const std::function<void(const std::string&)>& proc);

class ddimage
{
public:
    // image_type: .jpeg, .png等
    static bool support(const std::wstring& image_type);
    static void get_support_suffix(std::vector<std::wstring>& suffixs);

    // 创建图片
    // image_type: .jpeg, .png等
    // This file will not be occupied by this function.
    static bool create_from_path(const std::wstring& path, ddbitmap& bitmap);
    static bool create_from_buff(const std::wstring& image_type, const ddbuff& buff, ddbitmap& bitmap);

    // 保存到内存
    static bool save(const ddbitmap& bitmap, const std::wstring& image_type, ddbuff& buff);
    static  bool save(const ddbitmap& bitmap, const std::wstring& image_path);
    // 0 < width/height < 256 && width == height
    static bool save_as_ico(const ddbitmap& bitmap, ddbuff& buff, u32 width, u32 height);
    // 0 <= bright <= 255 bright越大图片越黑
    static bool save_as_wbmp(const ddbitmap& bitmap, ddbuff& buff, u8 bright);
};

//////////////////////////////////ddgif//////////////////////////////////
struct ddgif_frame_info
{
    ddbitmap bitmap;
    u32 frame_delay = 1000;
};

class ddgif
{
    ddgif() = default;
public:
    DDDEFAULT_COPY_MOVE(ddgif);
    ~ddgif();

    static std::unique_ptr<ddgif> create_empty();
    static std::unique_ptr<ddgif> create_from_path(const std::wstring& path);
    static std::unique_ptr<ddgif> create_from_buff(const ddbuff& buff);

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
    void* m_impl = nullptr;
};
} // namespace NSP_DD
#endif // ddimage_ddimage_h_
