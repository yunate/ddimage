#pragma warning(disable: 6297)

#include "ddimage/stdafx.h"
#include "ddimage/gif/ddgif_info_.h"

namespace NSP_DD {
#pragma region ddgif_memory_stream
u8* ddgif_memory_stream::seek(s32 count)
{
    u8* tmp = data;
    remain_size -= count;
    data += count;
    return tmp;
}

u8 ddgif_memory_stream::read_8()
{
    return *(u8*)(seek(sizeof(u8)));
}

u16 ddgif_memory_stream::read_16()
{
    return *(u16*)(seek(sizeof(u16)));
}

u32 ddgif_memory_stream::read_32()
{
    return *(u32*)(seek(sizeof(u32)));
}

void ddgif_memory_stream::write_8(u8 value)
{
    *(u8*)data = value;
    seek(sizeof(u8));
}

void ddgif_memory_stream::write_16(u16 value)
{
    *(u16*)data = value;
    seek(sizeof(u16));
}

void ddgif_memory_stream::write_32(u32 value)
{
    *(u32*)data = value;
    seek(sizeof(u32));
}

bool ddgif_memory_stream::read_len(s32& len)
{
    len = 0;
    u8 current_len = 0;
    while (true) {
        if (remain_size < 1) {
            return false;
        }
        current_len = read_8();
        if (current_len == 0) {
            return true;
        }
        seek(current_len);
        len += current_len;
    }

    return false;
}
#pragma endregion

#pragma region ddgif_signature
bool ddgif_signature::init(ddgif_memory_stream* stream)
{
    u8 gif89a[] = { 0x47, 0x49, 0x46, 0x38, 0x39, 0x61 };
    u8 gif87a[] = { 0x47, 0x49, 0x46, 0x38, 0x37, 0x61 };
    if (stream->remain_size < sizeof(gif89a)) {
        return false;
    }

    u8* sig = stream->seek(sizeof(gif89a));
    if (::memcmp(gif89a, sig, sizeof(gif89a)) == 0 ||
        ::memcmp(gif87a, sig, sizeof(gif87a)) == 0) {
        return true;
    }
    return false;
}

bool ddgif_signature::write(ddgif_memory_stream* stream)
{
    u8 gif89a[] = { 0x47, 0x49, 0x46, 0x38, 0x39, 0x61 };
    if (stream->remain_size < size()) {
        return false;
    }
    ::memcpy(stream->seek(sizeof(gif89a)), gif89a, sizeof(gif89a));
    return true;
}

s32 ddgif_signature::size()
{
    return 6;
}
#pragma endregion

#pragma region ddgif_logical_screen_desc
bool ddgif_logical_screen_desc::init(ddgif_memory_stream* stream)
{
    if (stream->remain_size < 7) {
        return false;
    }
    canvas_width = stream->read_16();
    canvas_height = stream->read_16();
    packed = stream->read_8();
    bgcolor_index = stream->read_8();
    pixel_aspect_ratio = stream->read_8();
    if (packed & 0x80) {
        color_table.resize(2 << (packed & 0x07));
        s32 byte_size = (s32)color_table.size() * sizeof(ddrgb);
        if (stream->remain_size < byte_size) {
            return false;
        }
        ::memcpy(color_table.data(), stream->seek(byte_size), byte_size);
    }
    return true;
}

bool ddgif_logical_screen_desc::write(ddgif_memory_stream* stream)
{
    if (stream->remain_size < size()) {
        return false;
    }
    stream->write_16(canvas_width);
    stream->write_16(canvas_height);
    stream->write_8(packed);
    stream->write_8(bgcolor_index);
    stream->write_8(pixel_aspect_ratio);
    if (!color_table.empty()) {
        s32 byte_size = (s32)color_table.size() * sizeof(ddrgb);
        ::memcpy(stream->seek(byte_size), color_table.data(), byte_size);
    }
    return true;
}

s32 ddgif_logical_screen_desc::size()
{
    return 7 + (s32)color_table.size() * sizeof(ddrgb);
}

ddrgb ddgif_logical_screen_desc::bgcolor()
{
    if (bgcolor_index >= color_table.size()) {
        return ddrgb();
    }
    return color_table[bgcolor_index];
}
#pragma endregion

#pragma region ddgif_image_desc
u8 ddgif_image_desc::disposal_method()
{
    return (ext_packed & 0x1c) >> 2;
}

bool ddgif_image_desc::have_transparent()
{
    return (ext_packed & 0x01) ? true : false;
}

bool ddgif_image_desc::interlaced()
{
    return (desc_packed & 0x40) ? true : false;
}

bool ddgif_image_desc::has_local_palette()
{
    return (desc_packed & 0x80) ? true : false;
}

bool ddgif_image_desc::is_iframe(const ddgif_logical_screen_desc& screen_desc)
{
    u8 disposal = disposal_method();
    if (disposal == 2) {
        return true;
    }

    if (disposal == 1
        && !have_transparent()
        && width == screen_desc.canvas_width
        && height == screen_desc.canvas_height) {
        return true;
    }
    return false;
}

bool ddgif_image_desc::init(ddgif_memory_stream* stream, const ddgif_logical_screen_desc& screen_desc)
{
    // 8
    if (stream->remain_size < 8) {
        return false;
    }
    left = stream->read_16();
    top = stream->read_16();
    width = stream->read_16();
    height = stream->read_16();

    s32 global_width = screen_desc.canvas_width;
    s32 global_height = screen_desc.canvas_height;
    if (width + left > global_width || height + top > global_height) {
        return false;
    }

    // color_table 1 + 
    if (stream->remain_size < 1) {
        return false;
    }
    desc_packed = stream->read_8();
    if (has_local_palette()) {
        color_table.resize(2 << (desc_packed & 0x07));
        s32 size = sizeof(ddrgb) * ((s32)color_table.size());
        if (stream->remain_size < size) {
            return false;
        }
        ::memcpy(color_table.data(), stream->seek(size), size);
    }

    // LZW code
    if (stream->remain_size < 1) {
        return false;
    }
    lzw_code_len = stream->read_8();
    if (lzw_code_len < 2 || lzw_code_len > 8) {
        return false;
    }

    ddgif_memory_stream sub_stream{ stream->data, stream->remain_size };
    while (true) {
        if (sub_stream.remain_size < 1) {
            return false;
        }
        u8 len = sub_stream.read_8();
        if (len == 0) {
            break;
        }
        if (sub_stream.remain_size < len) {
            return false;
        }
        sub_stream.seek(len);
    }
    s32 all_len = stream->remain_size - sub_stream.remain_size;
    raw_image_code.resize(all_len);
    ::memcpy(raw_image_code.data(), stream->seek(all_len), all_len);
    return true;
}

bool ddgif_image_desc::init_ext(ddgif_memory_stream* stream)
{
    if (stream->remain_size < 6) {
        return false;
    }

    // len 4
    u8 len = stream->read_8();
    if (len != 4) {
        return false;
    }

    // 1
    ext_packed = stream->read_8();

    // 2
    delay_time = stream->read_16() * 10;

    // 1
    transparent_color_index = stream->read_8();

    // len 0
    len = stream->read_8();
    has_ctrl_ext = true;
    return true;
}

bool ddgif_image_desc::write(ddgif_memory_stream* stream)
{
    // ext
    if (has_ctrl_ext) {
        u8 block_type = 0x21;
        stream->write_8(block_type);
        u8 ext_type = 0xf9;
        stream->write_8(ext_type);
        stream->write_8(4);
        stream->write_8(ext_packed);
        stream->write_16((u16)(delay_time / 10));
        stream->write_8(transparent_color_index);
        stream->write_8(0);
    }

    // desc
    u8 block_type = 0x2c;
    stream->write_8(block_type);

    stream->write_16(left);
    stream->write_16(top);
    stream->write_16(width);
    stream->write_16(height);

    stream->write_8(desc_packed);
    if (!color_table.empty()) {
        s32 size = (s32)color_table.size() * sizeof(ddrgb);
        ::memcpy(stream->seek(size), color_table.data(), size);
    }

    stream->write_8(lzw_code_len);
    DDASSERT(!raw_image_code.empty());
    if (!raw_image_code.empty()) {
        ::memcpy(stream->seek((s32)raw_image_code.size()), raw_image_code.data(), (s32)raw_image_code.size());
    }
    return true;
}

s32 ddgif_image_desc::size()
{
    s32 ext_size = 0;
    if (has_ctrl_ext) {
        ext_size = 2 + 6;
    }

    s32 desc_size = 1; // block type
    desc_size += 8; // left/top/width/height
    desc_size += (1 + (s32)color_table.size() * sizeof(ddrgb)); // desc_packed + color table
    desc_size += 1; // lzw_code_len
    desc_size += (s32)raw_image_code.size();
    return ext_size + desc_size;
}
#pragma endregion

#pragma region ddgif_app_extension
bool ddgif_app_extension::init(ddgif_memory_stream* stream)
{
    // (1 + 11) + (1 + 3) + 1
    if (stream->remain_size < 17) {
        return false;
    }
    ddgif_memory_stream sub_stream{ stream->seek(17), 17 };

    u8 len = sub_stream.read_8();
    if (len != 11) {
        return true;
    }

    char buf[11];
    ::memcpy(buf, sub_stream.seek(11), 11);
    if (memcmp(buf, "NETSCAPE2.0", 11) != 0 &&
        memcmp(buf, "ANIMEXTS1.0", 11) != 0) {
        return true;
    }

    len = sub_stream.read_8();
    if (len != 3) {
        return true;
    }

    //this should be 0x01 but isn't really important
    sub_stream.seek(1);
    loop_count = sub_stream.read_16();

    // len 0
    len = sub_stream.read_8();
    return true;
}

bool ddgif_app_extension::write(ddgif_memory_stream* stream)
{
    if (stream->remain_size < size()) {
        return false;
    }

    stream->write_8(0x21);
    stream->write_8(0xff);

    // (1 + 11)
    stream->write_8(11);
    ::memcpy(stream->seek(11), "NETSCAPE2.0", 11);

    //  (1 + 3)
    stream->write_8(3);
    stream->write_8(1);
    stream->write_16((u16)loop_count);

    // len 0
    stream->write_8(0);
    return true;
}

s32 ddgif_app_extension::size()
{
    return 2 + 17;
}
#pragma endregion

#pragma region ddgif_comment_extension
bool ddgif_comment_extension::init(ddgif_memory_stream* stream)
{
    while (true) {
        if (stream->remain_size < 1) {
            return false;
        }
        u8 len = stream->read_8();
        if (len == 0) {
            return true;
        }
        if (stream->remain_size < len) {
            return false;
        }
        comment.append((char*)stream->seek(len), len);
    }
    return false;
}

bool ddgif_comment_extension::write(ddgif_memory_stream* stream)
{
    if (stream->remain_size < size()) {
        return false;
    }
    stream->write_8(0x21);
    stream->write_8(0xf3);
    s32 comment_len = (s32)comment.length();
    u8* comment_data = (u8*)comment.data();
    while (true) {
        u8 len = 0;
        if (comment_len > 255) {
            len = 255;
        } else {
            len = (u8)comment_len;
        }
        stream->write_8(len);
        ::memcpy(stream->seek(len), comment_data, len);
        comment_len -= len;
        comment_data += len;
    }
    return true;
}

s32 ddgif_comment_extension::size()
{
    s32 comment_len = (s32)comment.length();
    s32 all_len = comment_len + (comment_len + 254) / 255 + 1;
    return 2 + all_len;
}
#pragma endregion

#pragma region ddgif_trailer
bool ddgif_trailer::init(ddgif_memory_stream* stream)
{
    if (stream->remain_size < 1) {
        return false;
    }

    // the block type have been readed, so here do not need read any more.
    // (void)stream->seek(1);
    return true;
}

bool ddgif_trailer::write(ddgif_memory_stream* stream)
{
    if (stream->remain_size < 1) {
        return false;
    }

    stream->write_8(0x3b);
    return true;
}

s32 ddgif_trailer::size()
{
    return 1;
}
#pragma endregion

#pragma region ddgif_info
ddgif_info::~ddgif_info()
{
    for (auto* it : image_descs) {
        delete it;
    }
    image_descs.clear();

    for (auto* it : comment_extensions) {
        delete it;
    }
    comment_extensions.clear();
}

bool ddgif_info::init(ddgif_memory_stream* stream)
{
    if (!signature.init(stream)) {
        return false;
    }
    if (!logical_screen_desc.init(stream)) {
        return false;
    }

    u8 block_type = 0;
    std::unique_ptr<ddgif_image_desc> image_desc = nullptr;
    while (true) {
        if (stream->remain_size < 1) {
            return false;
        }

        block_type = stream->read_8();
        if (block_type == 0x2c) { // image desc
            if (image_desc == nullptr) {
                image_desc.reset(new (std::nothrow) ddgif_image_desc());
                if (image_desc == nullptr) {
                    return false;
                }
            }

            if (!image_desc->init(stream, logical_screen_desc)) {
                return false;
            }

            image_descs.push_back(image_desc.release());
        } else if (block_type == 0x21) { // extension
            if (stream->remain_size < 1) {
                return false;
            }
            u8 ext = stream->read_8();
            if (ext == 0xf9) {
                if (image_desc != nullptr) {
                    // 0xf9 后面没有发现block_type == 0x2c
                    return false;
                }

                image_desc.reset(new (std::nothrow) ddgif_image_desc());
                if (image_desc == nullptr || !image_desc->init_ext(stream)) {
                    return false;
                }
            } else if (ext == 0xf3) { // comment extension
                ddgif_comment_extension* comment_extension = new (std::nothrow) ddgif_comment_extension();
                if (comment_extension == nullptr || !comment_extension->init(stream)) {
                    return false;
                }
                comment_extensions.push_back(comment_extension);
            } else if (ext == 0xff) { // application extension
                if (!app_extension.init(stream)) {
                    return false;
                }
            } else { // other extension
                // we do not support other extension.
                s32 len = 0;
                if (!stream->read_len(len)) {
                    return false;
                }
            }
        } else if (block_type == 0x3b) { // trailer
            return true;
        } else {
            return false;
        }
    }
    return true;
}

bool ddgif_info::write(ddgif_memory_stream* stream)
{
    if (!signature.write(stream)) {
        return false;
    }

    if (!logical_screen_desc.write(stream)) {
        return false;
    }

    if (!app_extension.write(stream)) {
        return false;
    }

    for (auto* it : image_descs) {
        if (!it->write(stream)) {
            return false;
        }
    }

    for (auto* it : comment_extensions) {
        if (!it->write(stream)) {
            return false;
        }
    }

    if (!trailer.write(stream)) {
        return false;
    }
    return true;
}

s32 ddgif_info::size()
{
    s32 size = 0;
    size += signature.size();
    size += logical_screen_desc.size();
    for (auto* it : image_descs) {
        size += it->size();
    }

    for (auto* it : comment_extensions) {
        size += it->size();
    }

    size += app_extension.size();
    size += trailer.size();
    return size;
}
#pragma endregion
} // namespace NSP_DD
