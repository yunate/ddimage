#include "ddimage_test/stdafx.h"
#include "ddbase/ddtest_case_factory.h"
#include "ddimage/ddimage.h"

#include "ddbase/ddtime.h"
#include "ddbase/ddio.h"

#include "ddwin/control/ddtrue_window.h"
#include "ddwin/ddkeyboard.h"
#include "ddwin/ddmouse.h"
#include "ddwin/ddwindow_utils.hpp"
#include "ddwin/ddmsg_loop.h"
#include "ddbase/windows/ddmoudle_utils.h"

namespace NSP_DD {
ddtimer g_timer;
class ddivisual
{
public:
    ddivisual()
    {
        m_bitmap1 = new ddbitmap();
        m_bitmap2 = new ddbitmap();
    }
    virtual ~ddivisual()
    {
        if (m_bitmap1 != nullptr) {
            delete m_bitmap1;
        }
        if (m_bitmap2 != nullptr) {
            delete m_bitmap2;
        }
    }

    virtual bool need_draw() = 0;

    virtual ddbitmap* get_bitmap()
    {
        render_border();
        return m_bitmap1;
    }

    virtual ddpoint get_offset()
    {
        return m_offset;
    }

    virtual void set_offset(const ddpoint& value)
    {
        m_offset = value;
    }

    virtual ddpoint get_size()
    {
        return m_size;
    }

    virtual void set_size(const ddpoint& value)
    {
        m_size = value;
    }

    virtual void swap_render_buff()
    {
        std::swap(m_bitmap1, m_bitmap2);

        if ((s32)m_bitmap1->width > m_size.x || (s32)m_bitmap1->height > m_size.y) {
            m_bitmap1->clip((m_bitmap1->width - m_size.x) / 2, (m_bitmap1->height - m_size.y) / 2,
                m_size.x, m_size.y);
        }

        m_bitmap1->flip_v();
        get_next_frame();
    }

    virtual void select(bool value)
    {
        if (value) {
            m_border_color = { 0, 0, 255, 255 };
            m_selected = true;
        } else {
            m_border_color = { 255, 255, 255, 255 };
            m_selected = false;
        }
    }

    virtual const ddpoint& selected_point()
    {
        return m_selected_point;
    }

    virtual void set_selected_point(const ddpoint& value)
    {
        m_selected_point = value - m_offset;
    }

    virtual bool is_selected()
    {
        return m_selected;
    }

    enum ddvisual_hittst_type {
        lt = 0,
        t,
        rt,
        l,
        r,
        lb,
        b,
        rb,
        in,
        out
    };
    virtual ddvisual_hittst_type hittest(const ddpoint& mouse_point)
    {
        ddrect rect{m_offset.x, m_offset.x  + m_size.x, m_offset.y, m_offset.y + m_size.y};
        if (rect.is_contian_point(mouse_point)) {
            return ddvisual_hittst_type::in;
        }
        return ddvisual_hittst_type::out;
    }

protected:
    virtual void get_next_frame() = 0;
    void render_border()
    {
        if (m_bitmap1 == nullptr || m_border_width == 0 || m_bitmap1->width < 100 || m_bitmap1->height < 100) {
            return;
        }

        for (s32 i = 0; i < (s32)m_bitmap1->width; ++i) {
            for (s32 j = 0; j < m_border_width; ++j) {
                m_bitmap1->at(i, j) = m_border_color;
                m_bitmap1->at(i, m_bitmap1->height - 1 - j) = m_border_color;
            }
        }

        for (s32 i = 0; i < (s32)m_bitmap1->height; ++i) {
            for (s32 j = 0; j < m_border_width; ++j) {
                m_bitmap1->at(j, i) = m_border_color;
                m_bitmap1->at(m_bitmap1->width - 1 - j, i) = m_border_color;
            }
        }

        s32 point_width = m_border_width;
        render_point(m_bitmap1->width / 2, point_width, point_width, m_border_color);
        render_point(m_bitmap1->width / 2, m_bitmap1->height - 1 - point_width, point_width, m_border_color);
        render_point(point_width, m_bitmap1->height / 2, point_width, m_border_color);
        render_point(m_bitmap1->width - 1- point_width, m_bitmap1->height / 2, point_width, m_border_color);
    }

    void render_point(s32 xx, s32 yy, s32 width, const ddbgra& color)
    {
        for (s32 x = xx - width; x <= xx + width; ++x) {
            for (s32 y = yy - width; y <= yy + width; ++y) {
                m_bitmap1->at(x, y) = color;
            }
        }
    }

    ddbitmap* m_bitmap1 = nullptr;
    ddbitmap* m_bitmap2 = nullptr;
    ddpoint m_offset = {0, 0};
    ddpoint m_size = {300, 300};
    s32 m_border_width = 2;
    ddbgra m_border_color = { 255, 255, 255, 255 };
    bool m_selected = false;
    ddpoint m_selected_point{0,0};
};

class ddgif_visual : public ddivisual
{
public:
    static std::unique_ptr<ddivisual> create_gif_visual(const std::wstring& full_path)
    {
        std::unique_ptr<ddgif_visual> visual = std::make_unique<ddgif_visual>();
        if (visual == nullptr) {
            return std::unique_ptr<ddivisual>();
        }
        visual->m_gif = ddgif::create_from_path(full_path);
        visual->get_next_frame();
        return std::move(visual);
    }

    bool need_draw() override
    {
        u64 this_time = g_timer.get_time_pass() / 1000000;
        if (m_pre_time + m_frame_delay < this_time) {
            m_pre_time = this_time;
            return true;
        }

        return false;
    }

    virtual void swap_render_buff()
    {
        m_frame_delay = m_next_frame_delay;
        ddivisual::swap_render_buff();
    }

    void get_next_frame() override
    {
        DDASSERT(m_gif != nullptr);
        ++m_current_frame;
        if (m_current_frame >= (s32)m_gif->get_frame_count()) {
            m_current_frame = 0;
        }
        ddgif_frame_info info;
        if (m_gif->get_frame(m_current_frame, info)) {
            *m_bitmap2 = std::move(info.bitmap);
            m_next_frame_delay = info.frame_delay;
        }
    }

private:
    std::unique_ptr<ddgif> m_gif;
    s32 m_current_frame = -1;
    u64 m_pre_time = 0;
    u32 m_frame_delay = 0;
    u32 m_next_frame_delay = 0;
};

class test_window : public ddtrue_window
{
public:
    ~test_window()
    {
    }
    test_window(const std::wstring& window_name) :
        ddtrue_window(window_name)
    {
        std::wstring base_path = ddpath::parent(ddstr::ansi_utf16(__FILE__));
        base_path = ddpath::join(base_path, L"test_folder");
        for (s32 i = 0; i < 50; ++i) {
            std::unique_ptr<ddivisual> it(ddgif_visual::create_gif_visual(ddpath::join(base_path, L"test_multi_page_image.gif")));
            if (it != nullptr) {
                m_visuals.push_back(std::move(it));
            }
        }

        MOUSE.ON_MOUSE = ([this](const DDMOUSE_EVENT& ev) {
            if (ev.type == DDMOUSE_EVENT::DDMOUSE_EVENT_TYPE::LDOWN) {
                set_title(ddstr::format(L"ON_LDOWN:pt:%d,%d", ev.relative_pos_x, ev.relative_pos_y).c_str());
                if (m_selected != nullptr) {
                    m_selected->select(false);
                    m_selected = nullptr;
                }

                for (size_t i = m_visuals.size(); i > 0; --i) {
                    auto& it = m_visuals[i - 1];
                    if (it->hittest({ ev.relative_pos_x, ev.relative_pos_y }) == ddivisual::ddvisual_hittst_type::in) {
                        m_selected = it.get();
                        m_selected->select(true);
                        m_selected->set_selected_point({ ev.relative_pos_x, ev.relative_pos_y });
                        break;
                    }
                }

                m_draging = true;
                m_repaint_request = true;
                return true;
            }
            if (ev.type == DDMOUSE_EVENT::DDMOUSE_EVENT_TYPE::LUP ||
                ev.type == DDMOUSE_EVENT::DDMOUSE_EVENT_TYPE::LEAVE) {
                set_title(ddstr::format(L"ON_LUP:pt:%d,%d", ev.relative_pos_x, ev.relative_pos_y).c_str());
                m_draging = false;
                return true;
            }
            if (ev.type == DDMOUSE_EVENT::DDMOUSE_EVENT_TYPE::MOVE) {
                if (!m_draging || m_selected == nullptr) {
                    return true;
                }
                m_selected->set_offset(ddpoint{ev.relative_pos_x, ev.relative_pos_y} - m_selected->selected_point());
                m_repaint_request = true;
                return true;
            }
            if (ev.type == DDMOUSE_EVENT::DDMOUSE_EVENT_TYPE::RDBCLICK) {
                set_title(ddstr::format(L"ON_LDBCLICK:pt:%d,%d", ev.relative_pos_x, ev.relative_pos_y).c_str());
                return true;
            }
            return true;
        });
    }

    DDWIN_PROC_CHAIN_DEFINE(test_window, ddtrue_window);
    bool win_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT&)
    {
        if (KB.on_msg(hwnd, uMsg, wParam, lParam)) {
            return true;
        }
        if (MOUSE.on_msg(hwnd, uMsg, wParam, lParam)) {
            return true;
        }

        switch (uMsg) {
            case WM_PAINT: {
                if (m_timer == 0) {
                    m_timer = ::SetTimer(get_window(), 0xffee, USER_TIMER_MINIMUM, NULL);
                }
                // ::InvalidateRect(get_window(), NULL, FALSE);
                draw(hwnd);
                break;
            }
            case WM_SIZE: {
                on_resieze();
                m_repaint_request = true;
                break;
            }
            case WM_TIMER: {
                switch (wParam) {
                    case 0xffee: {
                        bool need_draw = false;
                        for (auto& it : m_visuals) {
                            if (it->need_draw()) {
                                it->swap_render_buff();
                                need_draw = true;
                            }
                        }
                        if (need_draw || m_repaint_request) {
                            m_repaint_request = false;
                            ::InvalidateRect(get_window(), NULL, FALSE);
                        }
                    }
                }
                break;
            }
        }
        return false;
    }

public:
    ddkeyboard KB;
    ddmouse MOUSE;

private:
    void draw(HWND hwnd)
    {
        ddpoint size = get_size();
        for (s32 y = 0; y < size.y; ++y) {
            for (s32 x = 0; x < size.x; ++x) {
                m_bitmap.at(x, y) = { 255, 255, 255, 255 };
            }
        }

        for (auto& it : m_visuals) {
            ddbitmap* bitmap = it->get_bitmap();
            if (bitmap == nullptr) {
                continue;
            }

            ddpoint offset = it->get_offset();
            s32 y = offset.y > 0 ? offset.y : 0;
            s32 maxy = ((s32)bitmap->height + offset.y < size.y) ? ((s32)bitmap->height + offset.y) : size.y;
            s32 maxx = ((s32)bitmap->width + offset.x < size.x) ? ((s32)bitmap->width + offset.x) : size.x;
            s32 yy = 0;
            for (; y < maxy; ++y) {
                s32 x = offset.x > 0 ? offset.x : 0;
                s32 xx = 0;
                for (; x < maxx; ++x) {
                    m_bitmap.at(x, y) = bitmap->at(xx, yy);
                    ++xx;
                }
                ++yy;
            }
        }

        PAINTSTRUCT ps;
        HDC hdc = ::BeginPaint(hwnd, &ps);
        auto h_bitmap = ::CreateBitmap(m_bitmap.width, m_bitmap.height, 1, 32, m_bitmap.colors.data());
        if (h_bitmap != NULL) {
            HBRUSH brush = ::CreatePatternBrush(h_bitmap);
            if (brush != NULL) {
                RECT rect{ 0, 0, size.x, size.y };
                ::FillRect(hdc, &rect, brush);
                ::DeleteObject(brush);
            }
            ::DeleteObject(h_bitmap);
        }
        EndPaint(hwnd, &ps);
    }

    void on_resieze()
    {
        ddpoint size = get_size();
        m_bitmap.resize(size.x, size.y);

        if (m_visuals.empty()) {
            return;
        }
        ddpoint pre_offset = m_visuals[0]->get_offset();
        ddpoint pre_size = m_visuals[0]->get_size();
        for (size_t i = 1; i < m_visuals.size(); ++i) {
            ddivisual* cur = m_visuals[i].get();
            ddpoint cur_offset = cur->get_offset();
            ddpoint cur_size = cur->get_size();
            if (pre_offset.x + pre_size.x + cur_size.x <= size.x) {
                cur->set_offset({ pre_offset.x + pre_size.x , pre_offset .y});
            } else {
                cur->set_offset({ 0 , pre_offset.y + pre_size.y });
            }
            pre_offset = cur->get_offset();
            pre_size = cur->get_size();
        }
    }
    std::vector<std::unique_ptr<ddivisual>> m_visuals;
    u64 m_timer = 0;
    bool m_draging = false;
    ddivisual* m_selected = nullptr;

    ddbitmap m_bitmap;
    bool m_repaint_request = false;
};

DDTEST(test_case_ddgif_render_gif, 1)
{
    ::SetProcessDPIAware();
    test_window nativeWin(L"test_window");
    (void)nativeWin.init_window();
    nativeWin.set_pos({ 500, 200 });
    nativeWin.set_size({ 1000, 1000 });
    nativeWin.show();
    ddwin_msg_loop loop;
    loop.loop();
}
} // namespace NSP_DD
