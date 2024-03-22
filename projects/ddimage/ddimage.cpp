#include "ddimage/stdafx.h"
#include "ddimage/ddimage.h"
#include "ddimage/ddimage_log_.h"
#include "ddimage/ddimage_convert_.h"

#include "ddbase/ddio.h"
#include "ddbase/ddstatic_initer.h"

#include <FreeImage.h>
#include <memory>

namespace NSP_DD {
DDSTATIC_INITER(ddimage)
{
    init_ddimage_log();
    init_ddimage_default_convert_procs_map();
    FreeImage_Initialise();
}

DDSTATIC_DEINITER(ddimage)
{
    FreeImage_DeInitialise();
}

DDSTATIC_INIT(ddimage);

using ddunique_dib = std::unique_ptr<FIBITMAP, void(*)(FIBITMAP*)>;

static ddunique_dib ddmake_unique_dib(FIBITMAP* dib = nullptr)
{
    return ddunique_dib(dib, FreeImage_Unload);
}

static ddunique_dib ddcreate_freeimage_dib(const ddbitmap& bitmap)
{
    s32 w = bitmap.width;
    s32 h = bitmap.height;
    auto dib = ddmake_unique_dib(FreeImage_AllocateT(FIT_BITMAP, w, h, 32, 0, 0, 0));
    if (dib == nullptr) {
        return ddmake_unique_dib();
    }

    ddbgra* colors = (ddbgra*)(FreeImage_GetBits(dib.get()));
    if (colors == nullptr) {
        return ddmake_unique_dib();
    }

    (void)::memcpy(colors, bitmap.colors.data(), bitmap.colors.size() * sizeof(ddbgra));
    return dib;
}

////////////////////////////////////ddimage////////////////////////////////////
static bool skiped_fif(FREE_IMAGE_FORMAT fif)
{
    // 这个会内存泄露,暂时不支持
    if (fif == FIF_PSD) {
        return true;
    }

    return false;
}

bool ddimage::support(const std::wstring& image_type)
{
    FREE_IMAGE_FORMAT fif = FreeImage_GetFIFFromFilenameU(image_type.c_str());
    if (fif == FIF_UNKNOWN) {
        return false;
    }

    if (skiped_fif(fif)) {
        return false;
    }

    if (!FreeImage_FIFSupportsReading(fif) || !FreeImage_FIFSupportsWriting(fif)) {
        return false;
    }
    return true;
}

void ddimage::get_support_suffix(std::vector<std::wstring>& suffixs)
{
    for (int i = 0; i < FreeImage_GetFIFCount(); ++i) {
        FREE_IMAGE_FORMAT fif = (FREE_IMAGE_FORMAT)i;
        if (skiped_fif(fif)) {
            continue;
        }
        if (!FreeImage_IsPluginEnabled(fif)) {
            continue;
        }

        if (!FreeImage_FIFSupportsReading(fif) || !FreeImage_FIFSupportsWriting(fif)) {
            continue;
        }

        std::vector<std::wstring> support_suffixs;
        ddstr::split(ddstr::utf8_16(FreeImage_GetFIFExtensionList(fif)).c_str(), L",", support_suffixs);
        suffixs.insert(suffixs.end(), support_suffixs.begin(), support_suffixs.end());
    }
}

bool ddimage::create_from_path(const std::wstring& path, ddbitmap& bitmap)
{
    FREE_IMAGE_FORMAT fif = FreeImage_GetFIFFromFilenameU(path.c_str());
    if (fif == FIF_UNKNOWN) {
        return false;
    }

    auto dib = ddmake_unique_dib(FreeImage_LoadU(fif, path.c_str(), 0));
    if (dib == nullptr) {
        return false;
    }

    if (FreeImage_GetBPP(dib.get()) != 32) {
        dib = ddmake_unique_dib(FreeImage_ConvertTo32Bits(dib.get()));
        if (dib == nullptr) {
            return false;
        }
    }

    ddbgra* colors = (ddbgra*)(FreeImage_GetBits(dib.get()));
    if (colors == nullptr) {
        return false;
    }

    bitmap.resize(FreeImage_GetWidth(dib.get()), s32(FreeImage_GetHeight(dib.get())));
    (void)::memcpy(bitmap.colors.data(), colors, bitmap.colors.size() * sizeof(ddbgra));
    return true;
}

bool ddimage::create_from_buff(const std::wstring& image_type, const ddbuff& buff, ddbitmap& bitmap)
{
    FREE_IMAGE_FORMAT fif = FreeImage_GetFIFFromFilenameU(image_type.c_str());
    if (fif == FIF_UNKNOWN) {
        return false;
    }

    std::unique_ptr<FIMEMORY, void(*)(FIMEMORY*)> stream(FreeImage_OpenMemory((BYTE*)buff.data(), (DWORD)buff.size()), FreeImage_CloseMemory);
    if (stream == nullptr) {
        return false;
    }

    auto dib = ddmake_unique_dib(FreeImage_LoadFromMemory(fif, stream.get(), 0));
    if (dib == nullptr) {
        return false;
    }

    if (FreeImage_GetBPP(dib.get()) != 32) {
        dib = ddmake_unique_dib(FreeImage_ConvertTo32Bits(dib.get()));
        if (dib == nullptr) {
            return false;
        }
    }

    ddbgra* colors = (ddbgra*)(FreeImage_GetBits(dib.get()));
    if (colors == nullptr) {
        return false;
    }

    bitmap.resize(FreeImage_GetWidth(dib.get()), s32(FreeImage_GetHeight(dib.get())));
    (void)::memcpy(bitmap.colors.data(), colors, bitmap.colors.size() * sizeof(ddbgra));
    return true;
}

static bool save_t(const ddbitmap& bitmap, FREE_IMAGE_FORMAT fif, ddbuff& buff,
    const std::function<FIBITMAP*(FREE_IMAGE_FORMAT, FIBITMAP*)>& convert_proc)
{
    auto dib = ddcreate_freeimage_dib(bitmap);
    if (dib == nullptr) {
        return false;
    }

    // convert
    auto converted_dib = convert_proc(fif, dib.get());
    if (dib.get() != converted_dib) {
        dib.reset(converted_dib);
    }
    if (dib == nullptr) {
        return false;
    }

    FIMEMORY* memory = FreeImage_OpenMemory();
    if (memory == nullptr) {
        return false;
    }

    ddexec_guard guard([memory]() {
        FreeImage_CloseMemory(memory);
    });

    if (!FreeImage_SaveToMemory(fif, dib.get(), memory)) {
        return false;
    }

    BYTE* data = nullptr;
    DWORD size = 0;
    if (!FreeImage_AcquireMemory(memory, &data, &size)) {
        return false;
    }
    buff.resize(size);
    ::memcpy(buff.data(), data, size);
    return true;
}

bool ddimage::save(const ddbitmap& bitmap, const std::wstring& image_path)
{
    ddbuff buff;
    if (!save(bitmap, image_path, buff)) {
        return false;
    }

    std::unique_ptr<ddfile> file(ddfile::create_utf8_file(image_path));
    if (file == nullptr) {
        return false;
    }

    file->write((u8*)buff.data(), (s32)buff.size());
    file->resize((s32)buff.size());
    return true;
}

bool ddimage::save(const ddbitmap& bitmap, const std::wstring& image_type, ddbuff& buff)
{
    FREE_IMAGE_FORMAT fif = FreeImage_GetFIFFromFilenameU(image_type.c_str());
    if (fif == FIF_UNKNOWN) {
        fif = FIF_JPEG;
    }

    if (!FreeImage_FIFSupportsWriting(fif)) {
        ddimage_log(ddstr::format(L"do not support for .%s", ddpath::suffix(image_type).c_str()));
        return false;
    }

    return save_t(bitmap, fif, buff, [](FREE_IMAGE_FORMAT fif, FIBITMAP* dib) {
        auto convert = get_default_convert_proc(fif);
        if (convert != nullptr) {
            return convert(dib);
        }
        return dib;
    });
}

bool ddimage::save_as_ico(const ddbitmap& bitmap, ddbuff& buff, u32 width, u32 height)
{
    return save_t(bitmap, FIF_ICO, buff, [width, height](FREE_IMAGE_FORMAT, FIBITMAP* dib) {
        return convert_for_ico(dib, width, height);
    });
}

bool ddimage::save_as_wbmp(const ddbitmap& bitmap, ddbuff& buff, u8 bright)
{
    return save_t(bitmap, FIF_WBMP, buff, [bright](FREE_IMAGE_FORMAT, FIBITMAP* dib) {
        return convert_for_wbmp(dib, bright);
    });
}
} // namespace NSP_DD
