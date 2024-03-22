#include "ddimage/stdafx.h"
#include "ddimage/ddimage.h"
#include "ddimage/ddgif_.h"
#include "ddimage/ddimage_log_.h"

#include <memory>

namespace NSP_DD {
struct ddgif_memory_stream
{
    void seek(u32 offset)
    {
        // do not check offset
        data += offset;
        remain_size -= offset;
    }

    void read(u8* buff, u32 size)
    {
        ::memcpy(buff, data, size);
        seek(size);
    }

    void write(u8* buff, u32 size)
    {
        ::memcpy(data, buff, size);
        seek(size);
    }

    u8 read_u8()
    {
        u8 value = *data;
        seek(1);
        return value;
    }

    u16 read_u16()
    {
        u16 value = *(u16*)data;
        seek(2);
        return value;
    }

    u32 read_u32()
    {
        u32 value = *(u32*)data;
        seek(4);
        return value;
    }

    void write_u8(u8 value)
    {
        *data = value;
        seek(1);
    }

    void write_u16(u16 value)
    {
        *(u16*)data = value;
        seek(2);
    }

    void write_u32(u32 value)
    {
        *(u32*)data = value;
        seek(4);
    }

    bool get_sub_stream(ddgif_memory_stream& sub_stream, u32 size)
    {
        if (remain_size < size) {
            return false;
        }

        sub_stream = { data, size };
        seek(size);
        return true;
    }

    u8* data = nullptr;
    u32 remain_size = 0;
};
struct ddgif_comment_extexsion {
    bool init(ddgif_memory_stream* stream)
    {
        stream->seek(1); // block type
        stream->seek(1); // extension type

        if (stream->remain_size < 1) {
            // we do not think this is an error
            return false;
        }
        u8 b = stream->read_u8();
        if (b > stream->remain_size) {
            return false;
        }
        std::string current_comment(b, 0);
        stream->read((u8*)current_comment.data(), b);
        comment += current_comment;
        return true;
    }
    bool wirte(ddgif_memory_stream* stream)
    {
        stream->write_u8(0x21);  // block type
        stream->write_u8(0xff);  // extension type

        stream->write_u8((u8)comment.size());
        stream->write((u8*)comment.data(), (u32)comment.size());
        return true;
    }

    u32 size()
    {
        return 3 + (u32)comment.size();
    }
    std::string comment;
};
struct ddgif_app_extexsion {
    // we do not think this is an error.
    bool init(ddgif_memory_stream* stream, u8 packed)
    {
        if (stream->remain_size < 18) {
            return false;
        }

        ddgif_memory_stream sub_stream{ stream->data, 18 };
        stream->seek(18);

        sub_stream.seek(1); // block type
        sub_stream.seek(1); // extension type

        if (sub_stream.read_u8() != 11) {
            return true;
        }

        //Not everybody recognizes ANIMEXTS1.0 but it is valid
        if (memcmp(sub_stream.data, "NETSCAPE2.0", 11) != 0 &&
            memcmp(sub_stream.data, "ANIMEXTS1.0", 11) != 0) {
            return true;
        }
        sub_stream.seek(11);

        if (sub_stream.read_u8() != 3) {
            return true;
        }
        // this should be 0x01 but isn't really important
        sub_stream.seek(1);
        loop_count = sub_stream.read_u16();
        if (loop_count == 0) {
            loop_count = 1;
        }
        return true;
    }
    bool wirte(ddgif_memory_stream* stream, u8 packed)
    {
        stream->write_u8(0x21);  // block type
        stream->write_u8(0xfe);  // extension type

        stream->write_u8(11);
        stream->write((u8*)"NETSCAPE2.0", 11);
        stream->write_u8(3);
        stream->write_u8(0x01);
        stream->write_u16(loop_count);
        return true;
    }

    u32 size()
    {
        return 18;
    }
    u32 loop_count = 1; // 循环次数
};
struct ddgif_terminator {
    bool init(ddgif_memory_stream* stream, u8 packed)
    {
        return true;
    }

    bool wirte(ddgif_memory_stream* stream, u8 packed)
    {
        stream->write_u8(0x3b); // block type
        return true;
    }

    u32 size()
    {
        return 1;
    }
};
struct ddgif_control_extension {
    bool init(ddgif_memory_stream* stream, u8 packed)
    {
        // Graphic Control Extension
        desc.have_color_ext = true;
        // 1 + 2 + 1 + 1
        if (sub_stream.remain_size != 5) {
            return false;
        }

        desc.packed = sub_stream.read_u8();
        desc.have_transparent = (desc.packed & 0x01) ? true : false;
        desc.disposal_method = (desc.packed & 0x1c) >> 2;
        desc.delay_time = sub_stream.read_u16() * 10;
        desc.transparent_color_index = sub_stream.read_u8();

        stream->seek(1); // block type
        stream->seek(1); // extension type

        return true;
    }

    bool wirte(ddgif_memory_stream* stream, u8 packed)
    {
        stream->write_u8(0x21); // block type
        stream->write_u8(0xf9); // extension type
        return true;
    }

    u32 size()
    {
        return 1;
    }
};
struct ddgif_info {
    struct ddgif_signature
    {
        bool init(ddgif_memory_stream* stream)
        {
            // ASCII code for "GIF89a"
            static u8 gif89a[] = { 0x47, 0x49, 0x46, 0x38, 0x39, 0x61 };
            // ASCII code for "GIF87a"
            static u8 gif87a[] = { 0x47, 0x49, 0x46, 0x38, 0x37, 0x61 };

            if (stream->remain_size < sizeof(gif87a)) {
                return false;
            }

            stream->read(signature, sizeof(signature));
            if (::memcmp(gif89a, signature, 6) == 0 ||
                ::memcmp(gif87a, signature, 6) == 0) {
                return true;
            }
            return true;
        }

        bool wirte(ddgif_memory_stream* stream)
        {
            if (stream->remain_size < 6) {
                return false;
            }
            stream->write(signature, sizeof(signature));
            return true;
        }

        u32 size()
        {
            return 6;
        }

        u8 signature[6] = { 0 };
    } signature;

    struct ddgif_screen_desc
    {
        bool init(ddgif_memory_stream* stream)
        {
            if (stream->remain_size < 7) {
                return false;
            }

            // 2 width
            width = stream->read_u16();
            // 2 height
            height = stream->read_u16();
            // 1 packed
            packed = stream->read_u8();
            // 1 background_color
            background_color = stream->read_u8();
            // 1 aspect_ratio
            aspect_ratio = stream->read_u8();
            return true;
        }
        bool wirte(ddgif_memory_stream* stream)
        {
            if (stream->remain_size < 7) {
                return false;
            }

            // 2 width
            stream->write_u16(width);
            // 2 height
            stream->write_u16(height);
            // 1 packed
            stream->write_u8(packed);
            // 1 background_color
            stream->write_u8(background_color);
            // 1 aspect_ratio
            stream->write_u8(aspect_ratio);
            return true;
        }
        u32 size()
        {
            return 7;
        }
        u16 width = 0;
        u16 height = 0;
        u8 packed = 0;
        u8 background_color = 0;
        u8 aspect_ratio = 0;
    } screen_desc;

    struct ddgif_global_color_map
    {
        bool init(ddgif_memory_stream* stream)
        {
            if (!colors.empty()) {
                stream->read((u8*)colors.data(), colors.size() * sizeof(ddrgb));
            }
            return true;
        }

        bool wirte(ddgif_memory_stream* stream)
        {
            if (!colors.empty()) {
                stream->write((u8*)colors.data(), colors.size() * sizeof(ddrgb));
            }
            return true;
        }

        u32 size()
        {
            return colors.size() * sizeof(ddrgb);
        }
        std::vector<ddrgb> colors;
    } global_color_map;

    struct ddgif_image_descs
    {
        struct ddgif_image_desc {

            bool init(ddgif_memory_stream* stream, u8 packed)
            {
                stream->seek(1); // block type


                return true;
            }

            bool wirte(ddgif_memory_stream* stream, u8 packed)
            {
                return true;
            }

            u32 size()
            {
                return 1;
            }

            

            bool have_color_ext = false;
            bool have_transparent = false;
            s32 disposal_method = 0;
            u32 delay_time = 1000;
            u8 transparent_color_index = 0;
            u8 packed = 0;
        };

        bool init(ddgif_memory_stream* stream)
        {
            u8 block_type = 0;
            ddgif_image_desc desc;
            while (true) {
                if (stream->remain_size < 1) {
                    return false;
                }
                block_type = stream->read_u8();
                if (block_type == 0x2c) { // Image Descriptor
                    if (stream->remain_size < 9) {
                        return false;
                    }
                    u16 left = stream->read_u16();
                    u16 top = stream->read_u16();
                    u16 width = stream->read_u16();
                    u16 height = stream->read_u16();
                    u8 packed = stream->read_u8();
                    bool interlaced = (packed & 0x40) ? true : false;
                    bool no_local_palette = (packed & 0x80) ? false : true;

                    int bpp = 8;

                    //Local Color Table
                    if (packed & GIF_PACKED_ID_HAVELCT) {
                        io->seek_proc(handle, 3 * (2 << (packed & GIF_PACKED_ID_LCTSIZE)), SEEK_CUR);
                    }

                    //LZW Minimum Code Size
                    io->seek_proc(handle, 1, SEEK_CUR);


                    list.push_back(desc);
                    desc = ddgif_image_desc();
                } else if (block_type == 0x21) {  // Extension
                    if (stream->remain_size < 2) {
                        return false;
                    }
                    u8 ext = stream->read_u8();
                    u8 len = stream->read_u8();
                    if (stream->remain_size < len) {
                        return false;
                    }
                    ddgif_memory_stream sub_stream{ stream->data, len };
                    stream->seek(len);
                    if (ext == 0xf9) {
                        // Graphic Control Extension
                        desc.have_color_ext = true;
                        // 1 + 2 + 1 + 1
                        if (sub_stream.remain_size != 5) {
                            return false;
                        }

                        desc.packed = sub_stream.read_u8();
                        desc.have_transparent = (desc.packed & 0x01) ? true : false;
                        desc.disposal_method = (desc.packed & 0x1c) >> 2;
                        desc.delay_time = sub_stream.read_u16() * 10;
                        desc.transparent_color_index = sub_stream.read_u8();
                        // block terminator
                        sub_stream.seek(1);
                    } else if (ext == 0xfe) {
                        // Comment Extension
                    } else if (ext == 0xff) {
                        // Application Extension
                    }
                } else if (block_type == terminator) { // terminator
                    break;
                } else {
                    return false;
                }
            }

            return true;
        }
        std::list<ddgif_image_desc> list;
    } image_descs;

    std::list<ddgif_comment_extexsion> comment_extexsions;
    std::list<ddgif_app_extexsion> app_extexsions;
    ddgif_terminator terminator;
};

ddgif* ddgif::create_from_buff(const ddbuff& buff)
{
    std::unique_ptr<ddgif> gif = std::make_unique<ddgif>();
    if (!gif->init(buff)) {
        return nullptr;
    }
    return gif.release();
}


bool ddgif::init(const ddbuff& buff)
{
    ddgif_memory_stream sub_stream;
    ddgif_memory_stream stream{ (u8*)buff.data(), (u32)buff.size() };

    ddgif_info gif_info;
    if (!stream.get_sub_stream(sub_stream, 6) || !gif_info.signature.init(&sub_stream)) {
        return false;
    }

    if (!stream.get_sub_stream(sub_stream, 7) || !gif_info.screen_desc.init(&sub_stream)) {
        return false;
    }

    u32 size = (gif_info.screen_desc.packed & 0x80) ? (2 << (gif_info.screen_desc.packed & 0x07)) : 0;
    gif_info.global_color_map.colors.resize(size);
    if (!stream.get_sub_stream(sub_stream, size) || !gif_info.global_color_map.init(&sub_stream)) {
        return false;
    }

    u8 block_type = 0;
    while (true) {
        if (stream.remain_size < 1) {
            return false;
        }

        block_type = stream.read_u8();
        if (block_type == 0x2c) { // Image Descriptor
            //if (stream->remain_size < 9) {
            //    return false;
            //}
            //u16 left = stream->read_u16();
            //u16 top = stream->read_u16();
            //u16 width = stream->read_u16();
            //u16 height = stream->read_u16();
            //u8 packed = stream->read_u8();
            //bool interlaced = (packed & 0x40) ? true : false;
            //bool no_local_palette = (packed & 0x80) ? false : true;

            //int bpp = 8;

            ////Local Color Table
            //if (packed & GIF_PACKED_ID_HAVELCT) {
            //    io->seek_proc(handle, 3 * (2 << (packed & GIF_PACKED_ID_LCTSIZE)), SEEK_CUR);
            //}

            ////LZW Minimum Code Size
            //io->seek_proc(handle, 1, SEEK_CUR);


            //list.push_back(desc);
            //desc = ddgif_image_desc();
        } else if (block_type == 0x21) {  // Extension
            if (stream.remain_size < 2) {
                return false;
            }
            ddgif_memory_stream tmp_stream = stream;
            u8 ext = tmp_stream.read_u8();
            u8 len = tmp_stream.read_u8();
            while (len > 0) {
                if (tmp_stream.remain_size < len) {
                    return false;
                }
                tmp_stream.seek(len);
                if (tmp_stream.remain_size < 1) {
                    return false;
                }
                len = tmp_stream.read_u8();
            }

            stream.get_sub_stream(sub_stream, stream.remain_size - tmp_stream.remain_size);
            if (ext == 0xf9) {
                ddgif_control_extension control_extension;
                if (!control_extension.init(&sub_stream, desc.packed)) {
                    return false;
                }
            } else if (ext == 0xfe) {
                ddgif_comment_extexsion comment;
                if (!comment.init(&sub_stream)) {
                    return false;
                }
                gif_info.comment_extexsions.push_back(comment);
            } else if (ext == 0xff) {
                ddgif_app_extexsion app;
                if (!app.init(&sub_stream, desc.packed)) {
                    return false;
                }
                gif_info.app_extexsions.push_back(app);
            }
        } else if (block_type == 0x3b) { // terminator
            if (!stream.get_sub_stream(sub_stream, 0) || !gif_info.terminator.iniAAAAAAAAA AAL; IPKJBH U9N, MJ3.2
                ;*-
                .9**++++++++++++++++++++++++++++++++++++++++.``t(&sub_stream)) {
                return false;
            }
            return true;
        } else {
            return false;
        }
    }

    return false;
}

} // namespace NSP_DD
