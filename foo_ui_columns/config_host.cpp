#include "pch.h"

#include "config_host.h"
#include "prefs_utils.h"

void PreferencesTabsHost::make_child()
{
    destroy_child();

    RECT tab;

    GetWindowRect(m_wnd_tabs, &tab);
    MapWindowPoints(HWND_DESKTOP, m_wnd, (LPPOINT)&tab, 2);

    TabCtrl_AdjustRect(m_wnd_tabs, FALSE, &tab);

    if (m_active_tab >= (int)m_tab_count)
        m_active_tab = 0;

    if (m_active_tab < (int)m_tab_count && m_active_tab >= 0) {
        m_child = m_tabs[m_active_tab]->create(m_wnd);
    }

    // SetWindowPos(wnd_tab,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);

    if (m_child) {
        EnableThemeDialogTexture(m_child, ETDT_ENABLETAB);
    }

    SetWindowPos(m_child, nullptr, tab.left, tab.top, tab.right - tab.left, tab.bottom - tab.top, SWP_NOZORDER);
    SetWindowPos(m_wnd_tabs, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    ShowWindow(m_child, SW_SHOWNORMAL);
    // UpdateWindow(child);
}

INT_PTR PreferencesTabsHost::g_on_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    PreferencesTabsHost* p_instance;
    if (msg == WM_INITDIALOG) {
        p_instance = reinterpret_cast<PreferencesTabsHost*>(lp);
        SetWindowLongPtr(wnd, DWLP_USER, lp);
    } else
        p_instance = reinterpret_cast<PreferencesTabsHost*>(GetWindowLongPtr(wnd, DWLP_USER));
    return p_instance ? p_instance->on_message(wnd, msg, wp, lp) : FALSE;
}

namespace cui::prefs {
HWND PreferencesTabHelper::create(
    HWND wnd, UINT id, std::function<INT_PTR(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)> on_message_callback)
{
    m_on_message_callback = std::move(on_message_callback);
    return CreateDialogParam(
        mmh::get_current_instance(), MAKEINTRESOURCE(id), wnd, s_on_message, reinterpret_cast<LPARAM>(this));
}

INT_PTR PreferencesTabHelper::s_on_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    PreferencesTabHelper* p_instance;
    if (msg == WM_INITDIALOG) {
        p_instance = reinterpret_cast<PreferencesTabHelper*>(lp);
        SetWindowLongPtr(wnd, DWLP_USER, lp);
    } else {
        p_instance = reinterpret_cast<PreferencesTabHelper*>(GetWindowLongPtr(wnd, DWLP_USER));
    }
    return p_instance ? p_instance->on_message(wnd, msg, wp, lp) : FALSE;
}

INT_PTR PreferencesTabHelper::on_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_INITDIALOG:
        on_initdialog(wnd);
        break;
    case WM_NCDESTROY:
        on_ncdestroy();
        break;
    default:
        break;
    }
    return m_on_message_callback(wnd, msg, wp, lp);
}

void PreferencesTabHelper::on_initdialog(HWND wnd)
{
    m_wnd = wnd;
    m_title_font = create_default_title_font();

    const auto children = helpers::get_child_windows(wnd, [this, wnd](HWND wnd_child) {
        return GetAncestor(wnd_child, GA_PARENT) == wnd
            && m_title_ctrl_ids.find(GetDlgCtrlID(wnd_child)) != m_title_ctrl_ids.end();
    });
    for (auto&& child : children) {
        SetWindowFont(child, m_title_font, FALSE);
    }
}

void PreferencesTabHelper::on_ncdestroy()
{
    DeleteFont(m_title_font);
    m_title_font = nullptr;
    m_wnd = nullptr;
}

} // namespace cui::prefs

void PreferencesTabsHost::show_tab(const char* tab_name)
{
    const auto previous_tab = m_active_tab.get_value();

    for (size_t n = 0; n < m_tab_count; n++) {
        if (!strcmp(m_tabs[n]->get_name(), tab_name)) {
            m_active_tab = gsl::narrow<int>(n);
            break;
        }
    }

    if (m_wnd_tabs && previous_tab != m_active_tab) {
        TabCtrl_SetCurSel(m_wnd_tabs, m_active_tab);

        // See WM_WINDOWPOSCHANGED comment below
        if (IsWindowVisible(m_wnd))
            make_child();
    }

    ui_control::get()->show_preferences(get_guid());
}

INT_PTR PreferencesTabsHost::on_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_INITDIALOG: {
        m_wnd = wnd;
        m_wnd_tabs = GetDlgItem(wnd, IDC_TAB1);
        // SendMessage(wnd_tab, TCM_SETMINTABWIDTH, 0, 35);
        const auto count = m_tab_count;
        for (size_t n = 0; n < count; n++) {
            uTabCtrl_InsertItemText(m_wnd_tabs, gsl::narrow<int>(n), m_tabs[n]->get_name());
        }
        TabCtrl_SetCurSel(m_wnd_tabs, m_active_tab);
        make_child();
    } break;
    case WM_DESTROY:
        m_wnd_tabs = nullptr;
        m_wnd = nullptr;
        break;
    case WM_WINDOWPOSCHANGED: {
        auto lpwp = reinterpret_cast<LPWINDOWPOS>(lp);
        // Temporary workaround for various bugs that occur due to foobar2000 1.0+
        // having a dislike for destroying preference pages
        if (lpwp->flags & SWP_HIDEWINDOW) {
            destroy_child();
        } else if (lpwp->flags & SWP_SHOWWINDOW && !m_child) {
            make_child();
        }
    } break;
    case WM_NOTIFY:
        switch (((LPNMHDR)lp)->idFrom) {
        case IDC_TAB1:
            switch (((LPNMHDR)lp)->code) {
            case TCN_SELCHANGE: {
                m_active_tab = TabCtrl_GetCurSel(GetDlgItem(wnd, IDC_TAB1));
                make_child();
            } break;
            }
            break;
        }
        break;

    case WM_PARENTNOTIFY:
        switch (wp) {
        case WM_DESTROY: {
            if (m_child && (HWND)lp == m_child)
                m_child = nullptr;
        } break;
        }
        break;
    }
    return 0;
}
