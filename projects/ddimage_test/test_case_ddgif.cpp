#include "ddimage_test/stdafx.h"
#include "ddbase/ddtest_case_factory.h"
#include "ddimage/ddimage.h"

#include "ddbase/ddtime.h"
#include "ddbase/ddio.h"

namespace NSP_DD {
s32 expend_gif(const std::wstring& base_path, const std::wstring& file_name)
{
    std::wstring out_dir = ddpath::join(base_path, L"frames");
    if (dddir::is_path_exist(out_dir) && !dddir::is_dir(out_dir)) {
        dddir::delete_path(out_dir);
    }

    if (!dddir::is_path_exist(out_dir)) {
        dddir::create_dir_ex(out_dir);
    }

    ddtimer timer;
    std::wstring src_path = ddpath::join(base_path, file_name);
    std::unique_ptr<ddgif> image = ddgif::create_from_path(src_path);
    if (image == nullptr) {
        DDASSERT(false);
    }

    bool need_expanded = false;
    for (s32 i = 0; i < (s32)image->get_frame_count(); ++i) {
        if (!dddir::is_path_exist(ddpath::join(base_path, ddstr::format(L"frames\\%d.jpeg", i)))) {
            need_expanded = true;
            break;
        }
    }

    if (!need_expanded) {
        return image->get_frame_count();
    }

    ddcout(ddconsole_color::gray) << ddstr::format(L"open image cost: %dms\r\n", timer.get_time_pass() / 1000000);
    timer.reset();

    ddgif_frame_info info;
    for (s32 i = 0; i < (s32)image->get_frame_count(); ++i) {
        timer.reset();
        if (!image->get_frame(i, info)) {
            DDASSERT(false);
        }
        ddcout(ddconsole_color::gray) << ddstr::format(L"decode %d cost: %dms\r\n", i, timer.get_time_pass() / 1000000);
        ddimage::save(info.bitmap, ddpath::join(base_path, ddstr::format(L"frames\\%d.jpeg", i)));
    }

    return image->get_frame_count();
}

DDTEST(test_expend_ddgif, 1)
{
    std::wstring base_path = ddpath::parent(ddstr::ansi_utf16(__FILE__));
    base_path = ddpath::join(base_path, L"test_folder");
    std::wstring out_dir = ddpath::join(base_path, L"frames");
    if (dddir::is_path_exist(out_dir) && !dddir::is_dir(out_dir)) {
        dddir::delete_path(out_dir);
    }

    if (!dddir::is_path_exist(out_dir)) {
        dddir::create_dir_ex(out_dir);
    }

    ddtimer timer;
    std::wstring src_path = ddpath::join(base_path, L"test_multi_page_image1.gif");
    std::unique_ptr<ddgif> image(ddgif::create_from_path(src_path));
    if (image == nullptr) {
        DDASSERT(false);
    }

    ddcout(ddconsole_color::gray) << ddstr::format(L"open image cost: %dms\r\n", timer.get_time_pass() / 1000000);
    timer.reset();

    ddgif_frame_info info;
    for (s32 i = 0; i < (s32)image->get_frame_count(); ++i) {
        timer.reset();
        if (!image->get_frame(i, info)) {
            DDASSERT(false);
        }
        ddcout(ddconsole_color::gray) << ddstr::format(L"decode %d cost: %dms\r\n", i, timer.get_time_pass() / 1000000);
        ddimage::save(info.bitmap, ddpath::join(base_path, ddstr::format(L"frames\\%d.jpeg", i)));
    }
}

DDTEST(test_create_ddgif, 1)
{
    ddtimer timer;
    std::wstring base_path = ddpath::parent(ddstr::ansi_utf16(__FILE__));
    base_path = ddpath::join(base_path, L"test_folder");
    s32 count = expend_gif(base_path, L"test_multi_page_image.gif");
    //{
    //    std::unique_ptr<ddgif> image(ddgif::create_from_path(ddpath::join(base_path, L"111.gif")));
    //    std::unique_ptr<ddgif> image1(ddgif::create_from_path(ddpath::join(base_path, L"generated.gif")));
    //    
    //}

    std::unique_ptr<ddgif> gif(ddgif::create_empty());
    if (gif == nullptr) {
        DDASSERT(false);
    }
    gif->set_quality(7);
    for (s32 i = 0; i < count; ++i) {
        ddgif_frame_info info;
        if (!ddimage::create_from_path(ddpath::join(base_path, ddstr::format(L"frames\\%d.jpeg", i)), info.bitmap)) {
            DDASSERT(false);
        }
        info.frame_delay = 300;
        gif->insert_frame(info, -1);
    }

    for (s32 i = 0; i < count; ++i) {
        gif->set_time_delay(i, 40);
    }
    gif->save(ddpath::join(base_path, L"generated.gif"));
    ddcout(ddconsole_color::gray) << ddstr::format(L"create gif cost: %dms\r\n", timer.get_time_pass() / 1000000);
    timer.reset();
}

DDTEST(test_delete_ddgif, 1)
{
    ddtimer timer;
    std::wstring base_path = ddpath::parent(ddstr::ansi_utf16(__FILE__));
    base_path = ddpath::join(base_path, L"test_folder");

    std::unique_ptr<ddgif> image(ddgif::create_from_path(ddpath::join(base_path, L"test_multi_page_image.gif")));
    if (image == nullptr) {
        DDASSERT(false);
    }

    image->delete_frame(1);
    image->delete_frame(1);

    image->save(ddpath::join(base_path, L"deleted.gif"));
    ddcout(ddconsole_color::gray) << ddstr::format(L"delete gif's frame cost: %dms\r\n", timer.get_time_pass() / 1000000);
    timer.reset();
}

DDTEST(test_replace_ddgif, 1)
{
    ddtimer timer;
    std::wstring base_path = ddpath::parent(ddstr::ansi_utf16(__FILE__));
    base_path = ddpath::join(base_path, L"test_folder");

    std::unique_ptr<ddgif> image(ddgif::create_from_path(ddpath::join(base_path, L"test_multi_page_image.gif")));
    if (image == nullptr) {
        DDASSERT(false);
    }

    ddgif_frame_info info;
    if (!ddimage::create_from_path(ddpath::join(base_path, ddstr::format(L"test.jpg")), info.bitmap)) {
        DDASSERT(false);
    }

    info.frame_delay = 1000;
    image->replace_frame(info, 0);
    image->replace_frame(info, 1);
    image->replace_frame(info, 2);
    image->replace_frame(info, 3);

    image->save(ddpath::join(base_path, L"replaced.gif"));
    ddcout(ddconsole_color::gray) << ddstr::format(L"replaced gif's frame cost: %dms\r\n", timer.get_time_pass() / 1000000);
    timer.reset();
}
} // namespace NSP_DD
