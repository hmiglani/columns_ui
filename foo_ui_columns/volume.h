#ifndef _COLUMNS_VOLUME_H_
#define _COLUMNS_VOLUME_H_

/*!
 * \file volume.h
 *
 * \author musicmusic
 * \date 1 March 2015
 *
 * Volume control
 */

#include "main_window.h"

template <bool b_vertical, bool b_popup, typename t_attributes, class t_base = ui_helpers::container_window>
class VolumeBar
    : public t_base
    , private play_callback {
    class VolumeTrackBar : public uih::Trackbar {
        void get_channel_rect(RECT* rc) const override
        {
            if (b_popup)
                uih::Trackbar::get_channel_rect(rc);
            else {
                RECT rc_client;
                GetClientRect(get_wnd(), &rc_client);
                unsigned cx = calculate_thumb_size();

                rc->left = get_orientation() ? rc_client.left + 5 : rc_client.left + cx / 2;
                rc->right = get_orientation() ? rc_client.right - 5 : rc_client.right - cx + cx / 2;
                rc->top = get_orientation() ? rc_client.top + cx / 2 : rc_client.top + 5;
                rc->bottom = get_orientation() ? rc_client.bottom - cx + cx / 2 : rc_client.bottom - 5;
            }
        }

        void draw_background(HDC dc, const RECT* rc) const override
        {
            if (t_attributes::get_background_colour() != -1)
                FillRect(dc, rc, wil::unique_hbrush(CreateSolidBrush(t_attributes::get_background_colour())).get());
            else
                uih::Trackbar::draw_background(dc, rc);
        }
        /*void draw_thumb (HDC dc, const RECT * rc) const
        {
            if (get_uxtheme_ptr().is_valid() && get_theme_handle())
            {
                get_uxtheme_ptr()->DrawThemeBackground(get_theme_handle(), dc, get_orientation() ? TKP_THUMBVERT :
        TKP_THUMBBOTTOM, get_enabled() ? (get_tracking() ? TUS_PRESSED : (get_hot() ? TUS_HOT : TUS_NORMAL)) :
        TUS_DISABLED, rc, 0);
            }
            else
                uih::Trackbar::draw_thumb(dc, rc);
        }*/
        void draw_channel(HDC dc, const RECT* rc) const override
        {
            if (b_popup)
                uih::Trackbar::draw_channel(dc, rc);
            else {
                if (m_this->get_using_gdiplus()) {
                    Gdiplus::Graphics graphics(dc);
                    if (Gdiplus::Ok == graphics.GetLastStatus()) {
                        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

                        COLORREF cr_shadow = GetSysColor(COLOR_3DSHADOW);
                        COLORREF cr_hilight = GetSysColor(COLOR_3DHILIGHT);

                        RECT rcdraw(*rc);

                        Gdiplus::Color colour_hilight, colour_shadow;
                        colour_hilight.SetFromCOLORREF(cr_hilight);
                        colour_shadow.SetFromCOLORREF(cr_shadow);

                        Gdiplus::Pen pen_hilight(colour_hilight);
                        Gdiplus::Pen pen_shadow(colour_shadow);

                        graphics.DrawLine(&pen_shadow, Gdiplus::Point{rcdraw.left, rcdraw.bottom - 1},
                            Gdiplus::Point{rcdraw.right, rcdraw.top});
                        graphics.DrawLine(&pen_hilight, Gdiplus::Point{rcdraw.right, rcdraw.top},
                            Gdiplus::Point{rcdraw.right, rcdraw.bottom});
                        graphics.DrawLine(&pen_hilight, Gdiplus::Point{rcdraw.right, rcdraw.bottom},
                            Gdiplus::Point{rcdraw.left - 1, rcdraw.bottom});
                    }
                } else {
                    HPEN pn_light = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DHILIGHT));
                    HPEN pn_shadow = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));

                    HPEN pn_old = SelectPen(dc, pn_shadow);
                    MoveToEx(dc, rc->left, rc->bottom - 1, nullptr);
                    LineTo(dc, rc->right, rc->top);

                    SelectPen(dc, pn_light);
                    LineTo(dc, rc->right, rc->bottom);
                    LineTo(dc, rc->left - 1, rc->bottom);

                    SelectPen(dc, pn_old);

                    DeleteObject(pn_light);
                    DeleteObject(pn_shadow);
                }
            }
        }
        VolumeBar<b_vertical, b_popup, t_attributes, t_base>* m_this;

    public:
        VolumeTrackBar(VolumeBar<b_vertical, b_popup, t_attributes, t_base>* p_this) : m_this(p_this){};
    } m_child;

    class VolumeTrackBarCallback : public uih::TrackbarCallback {
        void on_position_change(unsigned pos, bool b_tracking) override
        {
            const auto volume = position_to_volume(pos);
            static_api_ptr_t<playback_control>()->set_volume(static_cast<float>(volume));
        }

        void get_tooltip_text(unsigned pos, uih::TrackbarString& out) override
        {
            const auto volume = position_to_volume(pos);
            out.append(pfc::stringcvt::string_os_from_utf8(pfc::format_float(volume, 0, 2)));
            out.append(_T(" dB"));
        }

        bool on_key(WPARAM wp, LPARAM lp) override
        {
            if (b_popup && wp == VK_ESCAPE && !(lp & (1 << 31))) {
                PostMessage(m_this->get_wnd(), WM_CLOSE, 0, 0);
                return true;
            }
            return false;
        }
        VolumeBar<b_vertical, b_popup, t_attributes, t_base>* m_this;

    public:
        VolumeTrackBarCallback(VolumeBar<b_vertical, b_popup, t_attributes, t_base>* p_this) : m_this(p_this){};
    } m_track_bar_host;

public:
    static double position_to_volume(const unsigned position)
    {
        const auto normalised = position / 1000.0;
        auto volume = 10.0 * std::log2(normalised);
        if (volume < -100.0)
            volume = -100;

        return volume;
    }

    static unsigned volume_to_position(const double volume)
    {
        auto position = std::lround(std::pow(2.0, volume / 10.0) * 1000.0);
        if (position < 0)
            position = 0;
        if (position > 1000)
            position = 1000;

        return gsl::narrow<unsigned>(position);
    }

    bool get_using_gdiplus() { return m_using_gdiplus; }
    HWND wnd_trackbar{nullptr};
    LRESULT on_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp) override
    {
        switch (msg) {
        case WM_CREATE: {
            if (!b_popup) {
                Gdiplus::GdiplusStartupInput gdiplusStartupInput;
                if (Gdiplus::Ok == Gdiplus::GdiplusStartup(&m_Gdiplus_token, &gdiplusStartupInput, nullptr))
                    m_using_gdiplus = true;
            }
            if (t_attributes::get_show_caption())
                m_font_caption.reset(uCreateMenuFont(b_vertical));

            m_child.set_callback(&m_track_bar_host);
            m_child.set_show_tooltips(true);

            wnd_trackbar = m_child.create(wnd);

            if (wnd_trackbar) {
                m_child.set_orientation(b_vertical);
                m_child.set_direction(b_vertical);
                m_child.set_range(1000);
                m_child.set_auto_focus(b_popup);
                m_child.set_scroll_step(20);
                m_child.set_mouse_wheel_direction(!b_vertical);
                update_position();
                if (b_popup)
                    SetFocus(wnd_trackbar);
            }
            ShowWindow(wnd_trackbar, SW_SHOWNORMAL);

            static_api_ptr_t<play_callback_manager>()->register_callback(
                this, play_callback::flag_on_volume_change, false);

            break;
        }
        case WM_SIZE: {
            if (t_attributes::get_show_caption()) {
                SIZE sz{};
                get_caption_extent(sz);
                unsigned size_caption = get_caption_size();
                const int x = b_vertical ? size_caption : sz.cx;
                const int y = 0;
                const int cx = LOWORD(lp) - (b_vertical ? size_caption : sz.cx);
                const int cy = HIWORD(lp);
                SetWindowPos(wnd_trackbar, nullptr, x, y, cx, cy, SWP_NOZORDER);
                RedrawWindow(wnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE);
            } else
                SetWindowPos(wnd_trackbar, nullptr, 0, 0, LOWORD(lp), HIWORD(lp), SWP_NOZORDER);
        } break;
        case WM_GETMINMAXINFO: {
            auto mmi = LPMINMAXINFO(lp);

            if (!b_popup) {
                if (!b_vertical)
                    mmi->ptMinTrackSize.y = uih::scale_dpi_value(21);
                mmi->ptMinTrackSize.x = uih::scale_dpi_value(50);
                // mmi->ptMaxTrackSize.y = 20;
            }
        }
            return 0;
        case WM_PRINTCLIENT: {
            if (lp & PRF_ERASEBKGND) {
                if (!b_popup) {
                    auto dc = (HDC)wp;
                    HWND wnd_parent = GetAncestor(wnd, GA_PARENT);
                    POINT pt = {0, 0}, pt_old = {0, 0};
                    MapWindowPoints(wnd, wnd_parent, &pt, 1);
                    OffsetWindowOrgEx(dc, pt.x, pt.y, &pt_old);
                    SendMessage(wnd_parent, msg, wp, lp);
                    SetWindowOrgEx(dc, pt_old.x, pt_old.y, nullptr);
                }
            }
        } break;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(wnd, &ps);

            if (t_attributes::get_show_caption()) {
                RECT rc_client, rc_dummy;
                GetClientRect(wnd, &rc_client);
                SIZE sz{};
                get_caption_extent(sz);
                auto size_caption = (long)get_caption_size();
                RECT rc_caption = {0, 0, b_vertical ? size_caption : sz.cx, rc_client.bottom};

                if (IntersectRect(&rc_dummy, &rc_caption, &ps.rcPaint)) {
                    HFONT old = SelectFont(dc, m_font_caption.get());
                    uDrawPanelTitle(dc, &rc_caption, "Volume", 6, b_vertical, false);
                    SelectFont(dc, old);
                }
            }
            EndPaint(wnd, &ps);
        }
            return 0;
        case WM_KEYDOWN: {
            auto lpkeyb = uih::GetKeyboardLParam(lp);
            if (b_popup && wp == VK_ESCAPE && !lpkeyb.transition_code && !lpkeyb.previous_key_state) {
                PostMessage(wnd, WM_CLOSE, 0, 0);
            }
        } break;
        case WM_CLOSE:
            DestroyWindow(wnd);
            return 0;
        case WM_ACTIVATE: {
            if (b_popup) {
                if (LOWORD(wp) == WA_INACTIVE)
                    PostMessage(wnd, WM_CLOSE, 0, 0);
            }
        } break;
        case WM_DESTROY: {
            static_api_ptr_t<play_callback_manager>()->unregister_callback(this);
            m_child.destroy();
            if (t_attributes::get_show_caption())
                m_font_caption.reset();

            if (!b_popup && m_using_gdiplus) {
                Gdiplus::GdiplusShutdown(m_Gdiplus_token);
                m_using_gdiplus = false;
                m_Gdiplus_token = NULL;
            }
        } break;
        }
        return DefWindowProc(wnd, msg, wp, lp);
    }

    VolumeBar() : m_child(this), m_track_bar_host(this) {}
    VolumeBar(const VolumeBar&) = delete;
    VolumeBar& operator=(const VolumeBar&) = delete;
    VolumeBar(VolumeBar&&) = delete;
    VolumeBar& operator=(VolumeBar&&) = delete;
    ~VolumeBar() = default;

    ui_helpers::container_window::class_data& get_class_data() const override
    {
        __implement_get_class_data_ex(t_attributes::get_class_name(), _T(""), !b_popup, 0,
            b_popup ? WS_POPUP | WS_CLIPCHILDREN | WS_BORDER : WS_CHILD | WS_CLIPCHILDREN,
            b_popup ? WS_EX_DLGMODALFRAME | WS_EX_TOOLWINDOW | WS_EX_TOPMOST : WS_EX_CONTROLPARENT, 0);
    }

    void update_position()
    {
        float vol = static_api_ptr_t<playback_control>()->get_volume();
        update_position(vol);
    }
    void update_position(float p_new_volume) { m_child.set_position(volume_to_position(p_new_volume)); }
    static unsigned g_get_caption_size(HFONT fnt)
    {
        if (!t_attributes::get_show_caption())
            return 0;
        unsigned rv = uGetFontHeight(fnt);
        rv += 9;
        return rv;
    }
    static unsigned g_get_caption_size()
    {
        if (!t_attributes::get_show_caption())
            return 0;
        wil::unique_hfont fnt(uCreateMenuFont());
        unsigned rv = g_get_caption_size(fnt.get());
        return rv;
    }

private:
    unsigned get_caption_size() const { return g_get_caption_size(m_font_caption.get()); }
    bool get_caption_extent(SIZE& p_out) const
    {
        HDC dc = GetDC(this->get_wnd());
        HFONT old = SelectFont(dc, m_font_caption.get());
        bool ret = uGetTextExtentPoint32(dc, "Volume", 6, &p_out) != 0;
        SelectFont(dc, old);
        ReleaseDC(this->get_wnd(), dc);
        return ret;
    }

    void FB2KAPI on_playback_starting(play_control::t_track_command p_command, bool p_paused) override{};
    void FB2KAPI on_playback_new_track(metadb_handle_ptr p_track) override{};
    void FB2KAPI on_playback_stop(play_control::t_stop_reason p_reason) override{};
    void FB2KAPI on_playback_seek(double p_time) override{};
    void FB2KAPI on_playback_pause(bool p_state) override{};
    void FB2KAPI on_playback_edited(metadb_handle_ptr p_track) override{};
    void FB2KAPI on_playback_dynamic_info(const file_info& p_info) override{};
    void FB2KAPI on_playback_dynamic_info_track(const file_info& p_info) override{};
    void FB2KAPI on_playback_time(double p_time) override{};
    void FB2KAPI on_volume_change(float p_new_val) override { update_position(p_new_val); }

    wil::unique_hfont m_font_caption;
    ULONG_PTR m_Gdiplus_token{NULL};
    bool m_using_gdiplus{false};
};

class PopupVolumeBarAttributes {
public:
    static const TCHAR* get_class_name() { return _T("volume_popup"); }
    static bool get_show_caption() { return true; }
    static COLORREF get_background_colour() { return -1; }
};

using volume_popup_t = VolumeBar<true, true, PopupVolumeBarAttributes>;

#endif
