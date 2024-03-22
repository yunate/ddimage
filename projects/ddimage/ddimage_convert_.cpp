#include "ddimage/stdafx.h"
#include "ddimage/ddimage.h"
#include "ddimage/ddimage_convert_.h"
#include "ddimage/ddimage_log_.h"

#include <memory>

namespace NSP_DD {
FIBITMAP* convert_for_wbmp(FIBITMAP* dib, u8 bright /* = 100 */)
{
    unsigned int width = FreeImage_GetWidth((FIBITMAP*)dib);
    unsigned int height = FreeImage_GetHeight((FIBITMAP*)dib);
    
    FIBITMAP* new_dib = FreeImage_AllocateT(FIT_BITMAP, width, height, 1);
    if (new_dib == nullptr) {
        return nullptr;
    }

    RGBQUAD* pal = FreeImage_GetPalette(new_dib);
    for (unsigned i = 0; i < FreeImage_GetColorsUsed(new_dib); i++) {
        pal[i].rgbRed = (BYTE)i;
        pal[i].rgbGreen = (BYTE)i;
        pal[i].rgbBlue = (BYTE)i;
    }

    ddbgra* raw_colors = (ddbgra*)FreeImage_GetBits((FIBITMAP*)dib);
    BYTE* img_bits = FreeImage_GetBits(new_dib);
    unsigned int pitch = FreeImage_GetPitch(new_dib);
    for (unsigned int y = 0; y < height; ++y) {
        BYTE* scanline = img_bits + y * pitch;
        for (unsigned int x = 0; x < width; ++x) {
            ddbgra color = raw_colors[y * width + x];
            BYTE value = (color.r + color.g + color.b) / 3 > bright ? 1 : 0;
            value ? scanline[x >> 3] |= (0x80 >> (x & 0x7)) : scanline[x >> 3] &= (0xFF7F >> (x & 0x7));
        }
    }

    return new_dib;
}

FIBITMAP* convert_for_ico(FIBITMAP* dib, s32 width /* = 48 */, s32 height /* = 48 */)
{
    return FreeImage_Rescale((FIBITMAP*)dib, width, height, FILTER_BOX);
}

static FIBITMAP* convert_for_pfm(FIBITMAP* dib)
{
    return FreeImage_ConvertToType((FIBITMAP*)dib, FIT_FLOAT, true);
}

static FIBITMAP* convert_for_gif(FIBITMAP* dib)
{
    return FreeImage_ColorQuantize((FIBITMAP*)dib, FIQ_WUQUANT);
}

static FIBITMAP* convert_for_hdr(FIBITMAP* dib)
{
    return FreeImage_ConvertToRGBF((FIBITMAP*)dib);
}

static FIBITMAP* convert_for_exr(FIBITMAP* dib)
{
    return FreeImage_ConvertToRGBAF((FIBITMAP*)dib);
}

template<void* T>
static FIBITMAP* convert_wrapper(FIBITMAP* dib)
{
    return T(dib);
}

static std::map<FREE_IMAGE_FORMAT, FREEIMAGE_CONVERT_PROC>& get_default_convert_procs_map()
{
    static std::map < FREE_IMAGE_FORMAT, FREEIMAGE_CONVERT_PROC> s_default_convert_procs_map;
    return s_default_convert_procs_map;
}

void init_ddimage_default_convert_procs_map()
{
    //get_default_convert_procs_map()[FIF_BMP] = FreeImage_ConvertTo32Bits;
    get_default_convert_procs_map()[FIF_ICO] = convert_wrapper<convert_for_ico>;
    get_default_convert_procs_map()[FIF_JPEG] = FreeImage_ConvertTo24Bits;
    //get_default_convert_procs_map()[FIF_JNG] = FreeImage_ConvertTo32Bits;
    get_default_convert_procs_map()[FIF_PBM] = FreeImage_ConvertTo24Bits;
    get_default_convert_procs_map()[FIF_PGM] = FreeImage_ConvertTo24Bits;
    //get_default_convert_procs_map()[FIF_PNG] = FreeImage_ConvertTo32Bits;
    get_default_convert_procs_map()[FIF_PPM] = FreeImage_ConvertTo24Bits;
    //get_default_convert_procs_map()[FIF_TARGA] = FreeImage_ConvertTo32Bits;
    //get_default_convert_procs_map()[FIF_TIFF] = FreeImage_ConvertTo32Bits;
    get_default_convert_procs_map()[FIF_WBMP] = convert_wrapper<convert_for_wbmp>;
    //get_default_convert_procs_map()[FIF_PSD] = FreeImage_ConvertTo32Bits;
    //get_default_convert_procs_map()[FIF_XPM] = FreeImage_ConvertTo32Bits;
    get_default_convert_procs_map()[FIF_EXR] = convert_for_exr;
    get_default_convert_procs_map()[FIF_GIF] = convert_for_gif;
    get_default_convert_procs_map()[FIF_HDR] = convert_for_hdr;
    //get_default_convert_procs_map()[FIF_J2K] = FreeImage_ConvertTo32Bits;
    //get_default_convert_procs_map()[FIF_JP2] = FreeImage_ConvertTo32Bits;
    get_default_convert_procs_map()[FIF_PFM] = convert_for_pfm;
    //get_default_convert_procs_map()[FIF_WEBP] = FreeImage_ConvertTo32Bits;
    //get_default_convert_procs_map()[FIF_JXR] = FreeImage_ConvertTo32Bits;
}

FREEIMAGE_CONVERT_PROC get_default_convert_proc(FREE_IMAGE_FORMAT fif)
{
    auto it = get_default_convert_procs_map().find(fif);
    if (it != get_default_convert_procs_map().end()) {
        return it->second;
    }

    return nullptr;
}

//const std::vector<CONVERT_PROC>& get_priority_sorted_converters()
//{
//    static std::vector<CONVERT_PROC> s_priority_sorted_converters = {
//        FreeImage_ConvertTo32Bits,
//        FreeImage_ConvertTo24Bits,
//        convert_to_8bit,
//        convert_to_16bit555,
//        convert_to_16bit565,
//        convert_to_64bit,
//        convert_to_48bit,
//        convert_to_4bit,
//    };
//    return s_priority_sorted_converters;
//}
} // namespace NSP_DD
