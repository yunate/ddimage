#ifndef ddimage_gif_ddgif_info__h_
#define ddimage_gif_ddgif_info__h_

#include "ddimage/ddimage.h"
#include "ddbase/ddmini_include.h"
#include "ddbase/ddcolor.h"

namespace NSP_DD {
struct ddgif_memory_stream
{
    u8* data = nullptr;
    s32 remain_size = 0;

    u8* seek(s32 count);
    u8 read_8();
    u16 read_16();
    u32 read_32();
    void write_8(u8 value);
    void write_16(u16 value);
    void write_32(u32 value);
    bool read_len(s32& len);
};

struct ddgif_signature
{
    bool init(ddgif_memory_stream* stream);
    bool write(ddgif_memory_stream* stream);
    s32 size();
};

struct ddgif_logical_screen_desc
{
    u16 canvas_width = 0;
    u16 canvas_height = 0;
    u8 packed = 0;
    u8 bgcolor_index = 0;
    u8 pixel_aspect_ratio = 0;
    std::vector<ddrgb> color_table;

    bool init(ddgif_memory_stream* stream);
    bool write(ddgif_memory_stream* stream);
    s32 size();
    ddrgb bgcolor();
};

struct ddgif_image_desc
{
    // control ext
    bool has_ctrl_ext = false;
    u8 ext_packed = 0;
    s32 delay_time = 66;
    u8 transparent_color_index = 0;

    // desc
    u16 left = 0;
    u16 top = 0;
    u16 width = 0;
    u16 height = 0;
    u8 desc_packed = 0;
    std::vector<ddrgb> color_table;
    u8 lzw_code_len = 0;
    ddbuff raw_image_code;

    u8 disposal_method();
    bool have_transparent();
    bool interlaced();
    bool has_local_palette();
    bool is_iframe(const ddgif_logical_screen_desc& screen_desc);

    bool init(ddgif_memory_stream* stream, const ddgif_logical_screen_desc& screen_desc);
    bool init_ext(ddgif_memory_stream* stream);
    bool write(ddgif_memory_stream* stream);
    s32 size();
};

struct ddgif_app_extension
{
    s32 loop_count = 0;

    bool init(ddgif_memory_stream* stream);
    bool write(ddgif_memory_stream* stream);
    s32 size();
};

struct ddgif_comment_extension
{
    std::string comment;
    bool init(ddgif_memory_stream* stream);
    bool write(ddgif_memory_stream* stream);
    s32 size();
};

struct ddgif_trailer
{
    bool init(ddgif_memory_stream* stream);
    bool write(ddgif_memory_stream* stream);
    s32 size();
};

struct ddgif_info
{
    ddgif_signature signature;
    ddgif_logical_screen_desc logical_screen_desc;
    std::vector<ddgif_image_desc*> image_descs;
    ddgif_app_extension app_extension;
    std::list<ddgif_comment_extension*> comment_extensions;
    ddgif_trailer trailer;

    ~ddgif_info();
    bool init(ddgif_memory_stream* stream);
    bool write(ddgif_memory_stream* stream);
    s32 size();
};
} // namespace NSP_DD
#endif // ddimage_gif_ddgif_info__h_
