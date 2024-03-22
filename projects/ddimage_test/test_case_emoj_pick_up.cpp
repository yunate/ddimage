#include "ddimage_test/stdafx.h"

#include "ddbase/ddtest_case_factory.h"
#include "ddbase/ddtime.h"
#include "ddbase/ddio.h"
#include "ddbase/thread/ddtask_thread.h"
#include "ddimage/ddimage.h"

#pragma warning(push, 1)
#include "3rd/nlohmann_json.hpp"
#pragma warning(pop)

namespace NSP_DD {
using json = nlohmann::json;

struct sprite_info
{
    std::wstring name;
    s32 x = 0;
    s32 y = 0;
    s32 w = 0;
    s32 h = 0;
};

static bool get_sprite_infos(const std::wstring& json_file_path, std::vector<sprite_info>& infos)
{
    std::unique_ptr<ddfile> file(ddfile::create_utf8_file(json_file_path));
    if (file == nullptr) {
        ddcout(ddconsole_color::red) << ddstr::format(L"get_get_sprite_part_infos failure!\r\n");
        return false;
    }

    std::string json_str(file->file_size(), 0);
    if ((s32)json_str.size() != file->read((u8*)json_str.data(), (s32)json_str.size())) {
        ddcout(ddconsole_color::red) << ddstr::format(L"file::read failure!\r\n");
        return false;
    }

    json data = json::parse(json_str);
    auto keymap = data["keyMap"];
    if (!keymap.is_object()) {
        ddcout(ddconsole_color::red) << ddstr::format(L"cannot find keyMap in %s file!\r\n", json_file_path.c_str());
        return false;
    }

    std::map<std::string, std::string> nick_names;
    for (auto it = keymap.begin(); it != keymap.end(); ++it) {
        std::string nick_name = it.key();
        if (it.value().is_string()) {
            std::string name = it.value();
            nick_names[name] = nick_name;
        }
    }

    auto coords = data["coords"];
    if (!coords.is_object()) {
        ddcout(ddconsole_color::red) << ddstr::format(L"cannot find coords in %s file!\r\n", json_file_path.c_str());
        return false;
    }

    for (auto it = coords.begin(); it != coords.end(); ++it) {
        sprite_info info;
        std::string name = it.key();
        auto nick_name = nick_names.find(it.key());
        if (nick_name == nick_names.end()) {
            ddcout(ddconsole_color::yellow) << ddstr::format("connot find %s's nick_name keyMap \r\n", name.c_str());
            continue;
        }

        info.name = ddstr::utf8_16(nick_name->second);
        if (!it.value().is_object()) {
            ddcout(ddconsole_color::yellow) << ddstr::format("read %s info failure!\r\n", name.c_str());
            continue;
        }

        auto x = it.value()["x"];
        auto y = it.value()["y"];
        auto w = it.value()["width"];
        auto h = it.value()["height"];
        if (!x.is_number_unsigned() || !y.is_number_unsigned() || !w.is_number_unsigned() || !h.is_number_unsigned()) {
            ddcout(ddconsole_color::yellow) << ddstr::format(L"read %s info failure!\r\n", info.name.c_str());
            continue;
        }

        info.x = (s32)x;
        info.y = (s32)y;
        info.w = (s32)w;
        info.h = (s32)h;
        infos.push_back(info);
    }
    return true;
}

static bool get_part(const ddbitmap* src, ddbitmap* dst, const ddrect& rect)
{
    s32 x = rect.l;
    s32 y = rect.r;
    s32 w = rect.t;
    s32 h = rect.b;
    y = (src->height - h - y);

    if (!(
        x < (s32)src->width && y < (s32)src->height &&
        x + w <= (s32)src->width && y + h <= (s32)src->height
    )) {
        return false;
    }

    dst->resize(w, h);

    for (s32 j = 0; j < h; ++j) {
        for (s32 i = 0; i < w; ++i) {
            dst->at(i, j) = src->at(i + x, j + y);
        }
    }
    return true;
}

DDTEST(teset_case_emoj_pick_up, 1)
{
    ddtask_thread_pool thread_pool{ 8 };

    std::wstring base_path = ddpath::join(ddpath::parent(ddstr::ansi_utf16(__FILE__)), L"test_folder/emoj");
    std::wstring base_dst_path = ddpath::join(base_path, L"out");
    if (!dddir::is_path_exist(base_dst_path)) {
        if (!dddir::create_dir_ex(base_dst_path)) {
            ddcout(ddconsole_color::red) << ddstr::format(L"create_dir_ex: %s failure!\r\n", base_dst_path.c_str());
            DDASSERT(false);
        }
    }

    ddtimer timer_all;
    ddtimer timer;

    // open
    std::wstring src_path = ddpath::join(base_path, L"sprite-min.png");
    ddbitmap src_image;
    if (!ddimage::create_from_path(src_path, src_image)) {
        ddcout(ddconsole_color::red) << ddstr::format(L"ddimage::create_from_path: %s failure!\r\n", src_path.c_str());
        DDASSERT(false);
    }

    ddcout(ddconsole_color::gray) << ddstr::format(L"open image cost: %dms\r\n", timer.get_time_pass() / 1000000);
    timer.reset();

    std::vector<sprite_info> infos;
    if (!get_sprite_infos(ddpath::join(base_path, L"sprite-meta.json"), infos)) {
        DDASSERT(false);
    }

    ddcout(ddconsole_color::gray) << ddstr::format(L"get_sprite_infos cost: %dms\r\n", timer.get_time_pass() / 1000000);
    timer.reset();
    std::atomic_char32_t count = (char32_t)infos.size();
    ddevent ev;

    for (size_t i = 0; i < infos.size(); ++i) {
        auto it = &infos[i];
        thread_pool.push_task([it, &src_image, &base_dst_path, &ev, &count]() {
            ddtimer timer;
            ddbitmap part_bitmap;
            ddrect rect{ it->x, it->y, it->w, it->h };
            if (!get_part(&src_image, &part_bitmap, rect)) {
                ddcout(ddconsole_color::red) << ddstr::format(L"get_part: %s failure!\r\n", it->name.c_str());
                DDASSERT(false);
            }

            std::wstring dst_path = ddpath::join({ base_dst_path, it->name + L".png" });
            if (!ddimage::save(part_bitmap, dst_path)) {
                ddcout(ddconsole_color::red) << ddstr::format(L"ddimage::save: %s failure!\r\n", dst_path.c_str());
                DDASSERT(false);
            }

            // ddcout(ddconsole_color::gray) << ddstr::format(L"pick up %s successful, cost: %dms\r\n", it.name.c_str(), timer.get_time_pass() / 1000000);
            if (count.fetch_sub(1) == char32_t(1)) {
                ev.notify();
            }
        });
    }

    ev.wait();
    ddcout(ddconsole_color::gray) << ddstr::format(L"successful, cost: %dms\r\n", timer_all.get_time_pass() / 1000000);
}
} // namespace NSP_DD
