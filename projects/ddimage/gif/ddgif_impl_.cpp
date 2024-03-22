
#pragma warning(disable: 28193)
#pragma warning(disable: 6297)

#include "ddimage/stdafx.h"
#include "ddimage/quantize/ddquantize_.h"
#include "ddimage/gif/ddgif_impl_.h"
#include "ddimage/gif/ddgif_string_table_.h"
#include "ddimage/ddimage_log_.h"
#include <memory>

namespace NSP_DD {
#pragma region ddgif_memory_stream
ddgif::~ddgif()
{
    if (m_impl != nullptr) {
        delete ((ddgif_impl*)m_impl);
    }
}

std::unique_ptr<ddgif> ddgif::create_empty()
{
    std::unique_ptr<ddgif> gif(new (std::nothrow) ddgif());
    if (gif == nullptr) {
        ddimage_log("out of memory, create ddgif failed.");
        return nullptr;
    }

    gif->m_impl = new (std::nothrow)ddgif_impl();
    if (gif->m_impl == nullptr) {
        return nullptr;
    }
    return gif;
}

std::unique_ptr<ddgif> ddgif::create_from_path(const std::wstring& path)
{
    std::unique_ptr<ddgif> gif(new (std::nothrow) ddgif());
    if (gif == nullptr) {
        ddimage_log("out of memory, create ddgif failed.");
        return nullptr;
    }

    gif->m_impl = ddgif_impl::create_from_path(path);
    if (gif->m_impl == nullptr) {
        return nullptr;
    }
    return gif;
}

std::unique_ptr<ddgif> ddgif::create_from_buff(const ddbuff& buff)
{
    std::unique_ptr<ddgif> gif(new (std::nothrow) ddgif());
    if (gif == nullptr) {
        ddimage_log("out of memory, create ddgif failed.");
        return nullptr;
    }

    gif->m_impl = ddgif_impl::create_from_buff(buff);
    if (gif->m_impl == nullptr) {
        return nullptr;
    }
    return gif;
}

u32 ddgif::get_frame_count()
{
    return ((ddgif_impl*)m_impl)->get_frame_count();
}

bool ddgif::get_frame(u32 index, ddgif_frame_info& info)
{
    return ((ddgif_impl*)m_impl)->get_frame(index, info);
}

void ddgif::set_time_delay(u32 index, s32 ms)
{
    ((ddgif_impl*)m_impl)->set_time_delay(index, ms);
}

void ddgif::set_quality(u8 quality)
{
    ((ddgif_impl*)m_impl)->set_quality(quality);
}

bool ddgif::insert_frame(const ddgif_frame_info& image, s32 index)
{
    return ((ddgif_impl*)m_impl)->insert_frame(image, index);
}

bool ddgif::delete_frame(u32 index)
{
    return ((ddgif_impl*)m_impl)->delete_frame(index);
}

bool ddgif::replace_frame(const ddgif_frame_info& image, u32 index)
{
    return ((ddgif_impl*)m_impl)->replace_frame(image, index);
}

bool ddgif::save(ddbuff& buff)
{
    return ((ddgif_impl*)m_impl)->save(buff);
}

bool ddgif::save(const std::wstring& path)
{
    return ((ddgif_impl*)m_impl)->save(path);
}
#pragma endregion

bool ddgif_impl::init(const ddbuff& buff)
{
    ddgif_memory_stream stream{(u8*)buff.data(), (s32)buff.size()};
    if (!m_gif_info.init(&stream)) {
        return false;
    }

    if (!m_gif_info.logical_screen_desc.color_table.empty()) {
        m_palette_size = (u16)m_gif_info.logical_screen_desc.color_table.size();
    } else if (!m_gif_info.image_descs.empty()) {
        m_palette_size = (u16)m_gif_info.image_descs[0]->size();
    }
    return true;
}

ddgif_impl* ddgif_impl::create_from_path(const std::wstring& path)
{
    if (!dddir::is_path_exist(path)) {
        return nullptr;
    }

    std::unique_ptr<ddfile> file(ddfile::create_utf8_file(path));
    if (file == nullptr) {
        return nullptr;
    }

    ddbuff buff;
    buff.resize(file->file_size());
    file->read((u8*)buff.data(), (s32)buff.size());
    return create_from_buff(buff);
}

ddgif_impl* ddgif_impl::create_from_buff(const ddbuff& buff)
{
    std::unique_ptr<ddgif_impl> image = std::make_unique<ddgif_impl>();
    if (image == nullptr) {
        return nullptr;
    }

    if (!image->init(buff)) {
        return nullptr;
    }
    return image.release();
}

u32 ddgif_impl::get_frame_count()
{
    return (u32)m_gif_info.image_descs.size();
}

bool ddgif_impl::get_frame(u32 index, ddgif_frame_info& info)
{
    ddgif_frame_info* catch_info = decode(index);
    if (catch_info != nullptr) {
        info = *catch_info;
        return true;
    }

    return false;
}

void ddgif_impl::set_time_delay(u32 index, s32 ms)
{
    if (index >= get_frame_count()) {
        ddimage_log(ddstr::format("There aren't so many frames, frame count:%d.", get_frame_count()));
        return;
    }
    m_gif_info.image_descs[index]->delay_time = ms;
}

void ddgif_impl::set_quality(u8 quality)
{
    if (quality < 2) {
        quality = 2;
    }

    if (quality > 8) {
        quality = 8;
    }
    m_palette_size = 1 << quality;
}

#define DDGIF_INTERLACE_PASSES 4
static u32 g_ddgif_interlace_offset[DDGIF_INTERLACE_PASSES] = { 0, 4, 2, 1 };
static u32 g_ddgif_interlace_increment[DDGIF_INTERLACE_PASSES] = { 8, 8, 4, 2 };

ddgif_frame_info* ddgif_impl::decode(u32 index)
{
    if (index >= get_frame_count()) {
        ddimage_log(ddstr::format("There aren't so many frames, frame count:%d.", get_frame_count()));
        return nullptr;
    }

    if (m_gif_info.image_descs[index] == m_catch_point) {
        return &m_catch_info;
    }

    std::unique_ptr<ddgif_string_table> string_table = std::make_unique<ddgif_string_table>();
    if (string_table == nullptr) {
        return nullptr;
    }

    u32 catch_index = get_catch_index();
    u32 begin_index = catch_index + 1;
    if (index < catch_index) {
        begin_index = 0;
        for (s32 i = (s32)index; i >= 0; --i) {
            if (m_gif_info.image_descs[i]->is_iframe(m_gif_info.logical_screen_desc)) {
                begin_index = i;
                break;
            }
        }
    }

    for (u32 i = begin_index; i <= index; ++i) {
        decode(m_gif_info.image_descs[i], string_table.get(), i == 0);
    }

    m_catch_point = m_gif_info.image_descs[index];
    m_catch_info.frame_delay = m_gif_info.image_descs[index]->delay_time;
    return &m_catch_info;
}

void ddgif_impl::decode(ddgif_image_desc* desc, ddgif_string_table* string_table, bool first_frame)
{
    DDASSERT(string_table != nullptr);
    DDASSERT(desc != nullptr);
    s32 width = desc->width;
    s32 height = desc->height;
    s32 global_width = m_gif_info.logical_screen_desc.canvas_width;
    s32 global_height = m_gif_info.logical_screen_desc.canvas_height;
    DDASSERT(width + desc->left <= global_width);
    DDASSERT(height + desc->top <= global_height);
    u8 disposal_method = desc->disposal_method();
    ddbitmap* bitmap = &m_catch_info.bitmap;
    if (first_frame || disposal_method == 2) {
        auto bgcolor = m_gif_info.logical_screen_desc.bgcolor();
        bitmap->resize(global_width, global_height);
        for (size_t i = 0; i < bitmap->colors.size(); ++i) {
            bitmap->colors[i].r = bgcolor.r;
            bitmap->colors[i].g = bgcolor.g;
            bitmap->colors[i].b = bgcolor.b;
            bitmap->colors[i].a = 0xff;
        }

        if (disposal_method == 2) {
            // 完全使背景颜色
            return;
        }
    } else if (disposal_method == 0 ||
        disposal_method == 1) {
        // 对比帧完全使用上一帧, 使用对比帧和del数据组合
    } else if (disposal_method == 3) {
        // 完全使用上一帧
        return;
    }

    // color table
    bool need_delete_color_table = false;
    ddrgb* color_table = nullptr;
    s32 color_table_size = 0;
    ddexec_guard guard([color_table, &need_delete_color_table]() {
        if (need_delete_color_table) {
            delete[]color_table;
        }
    });
    ddgif_logical_screen_desc global_info = m_gif_info.logical_screen_desc;
    if (desc->has_local_palette()) {
        color_table = desc->color_table.data();
        color_table_size = (s32)desc->color_table.size();
    } else if (!global_info.color_table.empty()) {
        color_table = global_info.color_table.data();
        color_table_size = (s32)global_info.color_table.size();
    } else {
        color_table = new ddrgb[256];
        color_table_size = 256;
        for (s32 i = 0; i < color_table_size; i++) {
            color_table[i].r = (u8)i;
            color_table[i].g = (u8)i;
            color_table[i].b = (u8)i;
        }
        need_delete_color_table = true;
    }

    u8 transparent_index = desc->transparent_color_index;
    bool interlaced = desc->interlaced();
    s32 x = 0;
    s32 y = 0;
    u8 interlacepass = 0;
    u16 bottom = (u16)(global_height - desc->top - height);
    ddbgra* scanline = &bitmap->colors[global_width * (height - y - 1 + bottom) + desc->left];
    ddgif_memory_stream stream{ desc->raw_image_code.data(), (s32)desc->raw_image_code.size() };
    u8 decode_buff[4096];
    int decode_size = sizeof(decode_buff);
    string_table->initialize(desc->lzw_code_len);
    while (true) {
        // the raw_image_code always have the right data
        DDASSERT(stream.remain_size >= 1);
        u8 len = stream.read_8();
        if (len == 0) {
            break;
        }
        DDASSERT(stream.remain_size >= len);
        ::memcpy(string_table->fill_input_buff(len), stream.seek(len), len);
        while (string_table->decompress(decode_buff, &decode_size)) {
            for (int i = 0; i < decode_size; i++) {
                u8 color_index = decode_buff[i];
                if (color_index == transparent_index && desc->have_transparent()) {
                    // keep the color
                } else if (color_index < color_table_size) {
                    scanline[x].r = color_table[color_index].r;
                    scanline[x].g = color_table[color_index].g;
                    scanline[x].b = color_table[color_index].b;
                    scanline[x].a = 0xff;
                }

                ++x;
                if (x >= width) {
                    if (interlaced) {
                        y += g_ddgif_interlace_increment[interlacepass];
                        if (y >= height) {
                            ++interlacepass;
                            if (interlacepass < DDGIF_INTERLACE_PASSES) {
                                y = g_ddgif_interlace_offset[interlacepass];
                            }
                        }
                    } else {
                        y++;
                    }

                    if (y == height) {
                        string_table->done();
                        break;
                    }

                    x = 0;
                    scanline = &bitmap->colors[global_width * (height - y - 1 + bottom) + desc->left];
                }
            }
            decode_size = sizeof(decode_buff);
        }
    }
}

static void ddgif_format_encoded_buff(ddbuff& dst, const ddbuff& src)
{
    size_t all = src.size();
    dst.resize((all + 254) / 255 + all + 1);
    size_t index = 0;
    for (size_t i = 0; i < all; ++i) {
        if (i % 255 == 0) {
            dst[index++] = (u8)((all - i >= 255) ? 255 : (all - i));
        }
        dst[index++] = src[i];
    }
    dst[index++] = 0;
}

ddgif_image_desc* ddgif_impl::create_image_desc(const ddbitmap* bitmap, s32 delay_time, const ddbitmap* pre_bitmap)
{
    DDASSERT(bitmap != nullptr);
    u16 canvas_width = m_gif_info.logical_screen_desc.canvas_width;
    u16 canvas_height = m_gif_info.logical_screen_desc.canvas_height;
    if (pre_bitmap != nullptr) {
        DDASSERT(pre_bitmap->width == canvas_width);
        DDASSERT(pre_bitmap->height == canvas_height);
    }
    const std::vector<ddbgra>& colors = bitmap->colors;
    u16 w = (u16)bitmap->width;
    u16 h = (u16)bitmap->height;
    u16 l = 0;
    u16 t = 0;

    if (w == 0 || h == 0) {
        ddimage_log("image's witdh and height cannot be 0.");
        return nullptr;
    }

    if (canvas_width < w || canvas_height < h) {
        // bitmap->clip((w - canvas_width) / 2, (h - canvas_height) / 2, canvas_width, canvas_height);
        ddimage_log(ddstr::format("the image's size[%d, %d] is greater than the canvas's size[%d, %d], you should clip the image's size",
            w, h, canvas_width, canvas_height));
        return nullptr;
    }

    // 居中对齐
    if (canvas_width > w) {
        l = (u16)(canvas_width - w) / 2;
    }

    if (canvas_height > h) {
        t = (u16)(canvas_height - h) / 2;
    }

    ddgif_image_desc* desc = new (std::nothrow)ddgif_image_desc();
    if (desc == nullptr) {
        ddimage_log("out of memory, create ddgif_image_desc failed.");
        return nullptr;
    }

    desc->raw_image_code.resize(1);
    desc->raw_image_code[0] = 0;
    desc->width = w;
    desc->height = h;
    desc->left = l;
    desc->top = t;

    ddbuff bits;
    if (!ddimage_quantize::wu_quantize(bitmap, m_palette_size - 1, desc->color_table, bits)) {
        ddimage_log("quantize image failed.");
        return nullptr;
    }

    s32 raw_size = (s32)desc->color_table.size();
    s32 color_table_size = (raw_size << 1 | raw_size) - raw_size;
    desc->color_table.resize(color_table_size);

    // move the transparent color to 0 index.
    desc->transparent_color_index = 0;
    desc->color_table[raw_size] = desc->color_table[0];

    bool have_trans = false;
    s32 trans_count = 0;
    desc->color_table[0] = { 0, 0, 0 };
    for (u32 i = 0; i < h; ++i) {
        for (u32 j = 0; j < w; ++j) {
            s32 index = i * w + j;
            if (bits[index] == 0) {
                bits[index] = (u8)raw_size;
            }

            const ddbgra& color = colors[index];
            if (color.a == 0) {
                ++trans_count;
                have_trans = true;
                bits[index] = desc->transparent_color_index;
                continue;
            }

            if (pre_bitmap != nullptr) {
                const ddbgra& color1 = pre_bitmap->colors[(i + t) * w + j + l];
                if (color == color1) {
                    ++trans_count;
                    have_trans = true;
                    bits[index] = desc->transparent_color_index;
                    continue;
                }
            }
        }
    }

    // control extension
    desc->has_ctrl_ext = true;
    desc->delay_time = delay_time;
    if (have_trans) {
        desc->ext_packed |= 0x01;
    }

    if (trans_count == w * h && pre_bitmap != nullptr) {
        // all frame's bits are transprant, set disposal_method = 3;
        desc->ext_packed |= (3 << 2) & 0x1c;
        desc->color_table.clear();
        return desc;
    } else {
        // disposal_method = 1;
        desc->ext_packed |= (1 << 2) & 0x1c;
    }

    // interlaced false
    desc->lzw_code_len = 0;
    while (color_table_size > 1) {
        color_table_size = color_table_size >> 1;
        if (color_table_size == 0) {
            break;
        }
        ++(desc->lzw_code_len);
    }

    desc->desc_packed = 0;
    if (desc->color_table != m_gif_info.logical_screen_desc.color_table) {
        // has local palette
        desc->desc_packed |= 0x80;
        desc->desc_packed |= (0x07 & (desc->lzw_code_len - 1));
    } else {
        desc->color_table.clear();
    }

    std::unique_ptr<ddgif_string_table> string_table = std::make_unique<ddgif_string_table>();
    if (string_table == nullptr) {
        ddimage_log("out of memory, create string table failed.");
        return nullptr;
    }
    string_table->initialize(desc->lzw_code_len);
    string_table->compress_start(8, w);

    ddbuff buff(bits.size());
    s32 encoded_size = 0;
    s32 size = (s32)buff.size();
    for (u32 y = 0; y < h; ++y) {
        ::memcpy(string_table->fill_input_buff(w), &bits[(h - y - 1) * w], w);
        while (string_table->compress(&buff[encoded_size], &size)) {
            encoded_size += size;
            size = (s32)buff.size() - encoded_size;
        }
    }

    encoded_size += string_table->compress_end(&buff[encoded_size]);
    buff.resize(encoded_size);
    ddgif_format_encoded_buff(desc->raw_image_code, buff);
    return desc;
}

void ddgif_impl::rebuild_logic_screen_desc()
{
    auto& screen_desc = m_gif_info.logical_screen_desc;
    if (m_gif_info.image_descs.empty()) {
        // reset the screen desc
        screen_desc = ddgif_logical_screen_desc();
        return;
    }

    ddgif_image_desc* desc = m_gif_info.image_descs[0];
    // no transparent color
    desc->ext_packed = 0;
    desc->transparent_color_index = 0;

    if (m_gif_info.image_descs.size() == 1) {
        screen_desc.canvas_width = desc->width;
        screen_desc.canvas_height = desc->height;
        ddrgb bgcolor = screen_desc.bgcolor();
        s32 bgcolor_index = -1;
        for (size_t i = 0; i < desc->color_table.size(); ++i) {
            if (desc->color_table[i] == bgcolor) {
                bgcolor_index = (s32)i;
                break;
            }
        }

        if (bgcolor_index != -1) {
            // copy the first frame's color table to the screen desc color table and
            // reset the first frame's color table.
            screen_desc.color_table = std::move(desc->color_table);
            screen_desc.packed = desc->desc_packed;
            desc->desc_packed &= 0x7f;
            screen_desc.bgcolor_index = (u8)bgcolor_index;
        }

        screen_desc.pixel_aspect_ratio = 0;
    }
}

bool ddgif_impl::insert_frame(const ddgif_frame_info& info, s32 index)
{
    if (index == -1 || index >= (s32)get_frame_count()) {
        index = (s32)get_frame_count();
    }

    if (get_frame_count() == 0) {
        // use the first page as the canvas_size
        m_gif_info.logical_screen_desc.canvas_width = (u16)info.bitmap.width;
        m_gif_info.logical_screen_desc.canvas_height = (u16)info.bitmap.height;
    }

    ddgif_image_desc* desc = nullptr;
    if (index == 0) {
        desc = create_image_desc(&info.bitmap, info.frame_delay, nullptr);
    } else {
        ddgif_frame_info pre_info;
        if (!get_frame(index - 1, pre_info)) {
            return false;
        }
        desc = create_image_desc(&info.bitmap, info.frame_delay, &pre_info.bitmap);
    }

    if (desc == nullptr) {
        ddimage_log("out of memory, create image desc failed.");
        return false;
    }

    if (index == (s32)get_frame_count() || m_gif_info.image_descs[index]->is_iframe(m_gif_info.logical_screen_desc)) {
        m_gif_info.image_descs.insert(m_gif_info.image_descs.begin() + index, desc);
    } else {
        // process the next frame
        ddgif_frame_info pre_catch = m_catch_info;
        ddgif_image_desc* catch_point = m_catch_point;
        ddgif_frame_info next_info;
        if (!get_frame(index, next_info)) {
            delete desc;
            return false;
        }

        // insert current frame and decode it.
        m_gif_info.image_descs.insert(m_gif_info.image_descs.begin() + index, desc);
        ddexec_guard guard([this, index, desc]() {
            m_gif_info.image_descs.erase(m_gif_info.image_descs.begin() + index);
            delete desc;
        });
        m_catch_info = pre_catch;
        m_catch_point = catch_point;
        ddgif_frame_info current_info;
        if (!get_frame(index, current_info)) {
            return false;
        }

        ddgif_image_desc* desc_next = create_image_desc(&next_info.bitmap, next_info.frame_delay, &current_info.bitmap);
        if (desc_next == nullptr) {
            ddimage_log("out of memory, create image desc failed.");
            return false;
        }

        std::swap(desc_next, m_gif_info.image_descs[index + 1]);
        delete desc_next;
        guard.clear();
    }

    if (index == 0) {
        rebuild_logic_screen_desc();
    }
    return true;
}

bool ddgif_impl::delete_frame(u32 index)
{
    if (index >= get_frame_count()) {
        ddimage_log(ddstr::format("There aren't so many frames, frame count:%d.", get_frame_count()));
        return false;
    }

    if (index == get_frame_count() - 1) {
        delete m_gif_info.image_descs[index];
        m_gif_info.image_descs.pop_back();
        return true;
    }

    std::unique_ptr<ddgif_frame_info> pre_info;
    if (index != 0) {
        pre_info = std::make_unique<ddgif_frame_info>();
        if (pre_info == nullptr) {
            ddimage_log("out of memory, create ddgif_frame_info failed.");
            return false;
        }

        if (!get_frame(index - 1, *pre_info)) {
            return false;
        }
    }

    // get the next frame
    ddgif_frame_info next_info;
    if (!get_frame(index + 1, next_info)) {
        return false;
    }

    ddgif_image_desc* desc_next = create_image_desc(&next_info.bitmap, next_info.frame_delay,
        (pre_info != nullptr) ? &pre_info->bitmap : nullptr);
    if (desc_next == nullptr) {
        return false;
    }
    std::swap(desc_next, m_gif_info.image_descs[index + 1]);
    delete desc_next;
    delete m_gif_info.image_descs[index];
    m_gif_info.image_descs.erase(m_gif_info.image_descs.begin() + index);

    if (index == 0) {
        rebuild_logic_screen_desc();
    }
    return true;
}

bool ddgif_impl::replace_frame(const ddgif_frame_info& image, u32 index)
{
    if (index >= get_frame_count()) {
        ddimage_log(ddstr::format("There aren't so many frames, frame count:%d.", get_frame_count()));
        return false;
    }

    ddgif_image_desc* desc = nullptr;
    if (index == 0) {
        desc = create_image_desc(&image.bitmap, image.frame_delay, nullptr);
    } else {
        ddgif_frame_info pre_info;
        if (!get_frame(index - 1, pre_info)) {
            return false;
        }
        desc = create_image_desc(&image.bitmap, image.frame_delay, &pre_info.bitmap);
    }

    if (desc == nullptr) {
        return false;
    }

    if (index == get_frame_count() - 1 || m_gif_info.image_descs[index + 1]->is_iframe(m_gif_info.logical_screen_desc)) {
        std::swap(desc, m_gif_info.image_descs[index]);
        delete desc;
    } else {
        // process the next frame
        ddgif_frame_info pre_catch = m_catch_info;
        ddgif_image_desc* catch_point = m_catch_point;
        ddgif_frame_info next_info;
        if (!get_frame(index + 1, next_info)) {
            delete desc;
            return false;
        }

        // replace current
        std::swap(desc, m_gif_info.image_descs[index]);
        ddexec_guard guard([this, index, &desc]() {
            std::swap(desc, m_gif_info.image_descs[index]);
            delete desc;
        });
        m_catch_info = pre_catch;
        m_catch_point = catch_point;
        ddgif_frame_info current_info;
        if (!get_frame(index, current_info)) {
            return false;
        }

        ddgif_image_desc* desc_next = create_image_desc(&next_info.bitmap, next_info.frame_delay, &current_info.bitmap);
        if (desc_next == nullptr) {
            ddimage_log("out of memory, create image desc failed.");
            return false;
        }

        std::swap(desc_next, m_gif_info.image_descs[index + 1]);
        delete desc_next;
        delete desc;
        guard.clear();
    }

    if (index == 0) {
        rebuild_logic_screen_desc();
    }
    return true;
}

bool ddgif_impl::save(ddbuff& buff)
{
    buff.resize(m_gif_info.size());
    ddgif_memory_stream stream{ (u8*)buff.data(), (s32)buff.size() };
    return m_gif_info.write(&stream);
}

bool ddgif_impl::save(const std::wstring& path)
{
    if (get_frame_count() == 0) {
        ddimage_log("There are no frames in the gif.");
        return false;
    }
    std::unique_ptr<ddfile> file(ddfile::create_utf8_file(path));
    if (file == nullptr) {
        return false;
    }

    ddbuff buff;
    if (!save(buff)) {
        return false;
    }

    if (file->write(buff.data(), (s32)buff.size()) != (s32)buff.size()) {
        return false;
    }
    file->resize((s32)buff.size());
    return true;
}

u32 ddgif_impl::get_catch_index()
{
    for (size_t i = 0; i < m_gif_info.image_descs.size(); ++i) {
        if (m_gif_info.image_descs[i] == m_catch_point) {
            return (u32)i;
        }
    }

    return (u32)m_gif_info.image_descs.size();
}
} // namespace NSP_DD
