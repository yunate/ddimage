#include "ddimage_test/stdafx.h"

#include "ddbase/ddtest_case_factory.h"
#include "ddbase/ddtime.h"
#include "ddbase/ddio.h"
#include "ddimage/ddimage.h"

namespace NSP_DD {
static bool save_buff_to_file(const std::wstring& file_path, const ddbuff& buff)
{
    std::unique_ptr<ddfile> file(ddfile::create_utf8_file(file_path));
    if (file == nullptr) {
        return false;
    }

    file->write((u8*)buff.data(), (s32)buff.size());
    return true;
}

DDTEST(test_ddimage_save_to_any_type, 1)
{
    std::wstring base_path = ddpath::parent(ddstr::ansi_utf16(__FILE__));
    base_path = ddpath::join(base_path, L"test_folder");

    // open
    std::wstring src_path = ddpath::join(base_path, L"test.jpg");
    ddbitmap src_image;
    if (!ddimage::create_from_path(src_path, src_image)) {
        DDASSERT(false);
    }

    // save
    std::vector<std::wstring> suffixs;
    ddimage::get_support_suffix(suffixs);
    for (size_t i = 0; i < suffixs.size(); ++i) {
        std::wstring dst_full_path = ddpath::join(base_path, L"test1.") + suffixs[i];
        ddbuff buff;
        if (ddimage::save(src_image, dst_full_path, buff) && save_buff_to_file(dst_full_path, buff)) {
            ddcout(ddconsole_color::green) << ddstr::format(L"%d %s, successful\r\n", i, suffixs[i].c_str());
        } else {
            ddcout(ddconsole_color::red) << ddstr::format(L"%d %s, failure\r\n", i, suffixs[i].c_str());
            dddir::delete_path(dst_full_path);
        }
    }
}

DDTEST(test_ddimage_modify, 1)
{
    std::wstring base_path = ddpath::parent(ddstr::ansi_utf16(__FILE__));
    base_path = ddpath::join(base_path, L"test_folder");

    ddtimer timer;

    // open
    std::wstring src_path = ddpath::join(base_path, L"test.jpg");
    ddbitmap src_image;
    if (!ddimage::create_from_path(src_path, src_image)) {
        DDASSERT(false);
    }
    ddcout(ddconsole_color::gray) << ddstr::format(L"open image cost: %dms\r\n", timer.get_time_pass() / 1000000);
    timer.reset();

    ddbitmap& bitmap = src_image;
    s32 w = bitmap.width;
    s32 h = bitmap.height;
    for (s32 y = 0; y < h; ++y) {
        for (s32 x = 0; x < w; ++x) {
            s32 i = y * w + x;
            if (x > (w - 100) / 2 && x < (w + 100) / 2 &&
                y >(h - 100) / 2 && y < (h + 100) / 2) {
                bitmap.colors[i].a = 0;
                bitmap.colors[i].r = 0;
                bitmap.colors[i].g = 0;
                bitmap.colors[i].b = 0;
            }
        }
    }
    ddcout(ddconsole_color::gray) << ddstr::format(L"modify color cost: %dms\r\n", timer.get_time_pass() / 1000000);
    timer.reset();

    {
        std::wstring dst_path = ddpath::join(base_path, L"test1.jpg");
        ddbuff buff;
        if (!ddimage::save(src_image, dst_path, buff) || !save_buff_to_file(dst_path, buff)) {
            DDASSERT(false);
        }
        ddcout(ddconsole_color::gray) << ddstr::format(L"save to test1.jpg cost: %dms\r\n", timer.get_time_pass() / 1000000);
        timer.reset();
    }

    {
        std::wstring dst_path = ddpath::join(base_path, L"test1.ico");
        ddbuff buff;
        if (!ddimage::save_as_ico(src_image, buff, 128, 128) || !save_buff_to_file(dst_path, buff)) {
            DDASSERT(false);
        }
        ddcout(ddconsole_color::gray) << ddstr::format(L"save to test1.ico cost: %dms\r\n", timer.get_time_pass() / 1000000);
        timer.reset();
    }

    {
        std::wstring dst_path = ddpath::join(base_path, L"test2.wbmp");
        ddbuff buff;
        if (!ddimage::save_as_wbmp(src_image, buff, 80) || !save_buff_to_file(dst_path, buff)) {
            DDASSERT(false);
        }
        ddcout(ddconsole_color::gray) << ddstr::format(L"save to test2.wbmp cost: %dms\r\n", timer.get_time_pass() / 1000000);
        timer.reset();

        ddbitmap src_image1;
        if (!ddimage::create_from_path(dst_path, src_image1)) {
            DDASSERT(false);
        }
        ddcout(ddconsole_color::gray) << ddstr::format(L"open test2.wbmp cost: %dms\r\n", timer.get_time_pass() / 1000000);
        timer.reset();

        std::wstring dst_path1 = ddpath::join(base_path, L"test2.jpeg");
        if (!ddimage::save(src_image1, dst_path1, buff) || !save_buff_to_file(dst_path1, buff)) {
            DDASSERT(false);
        }
        ddcout(ddconsole_color::gray) << ddstr::format(L"save to test2.jpeg cost: %dms\r\n", timer.get_time_pass() / 1000000);
        timer.reset();
    }
}
} // namespace NSP_DD
