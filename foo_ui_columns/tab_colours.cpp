#include "stdafx.h"

#include "tab_colours.h"
#include "main_window.h"

bool TabColours::is_active()
{
    return m_wnd != nullptr;
}

bool TabColours::get_help_url(pfc::string_base& p_out)
{
    p_out = "http://yuo.be/wiki/columns_ui:config:colours_and_fonts:colours";
    return true;
}

const char* TabColours::get_name()
{
    return "Colours";
}

HWND TabColours::create(HWND wnd)
{
    return m_helper.create(
        wnd, IDD_PREFS_COLOURS, [this](auto&&... args) { return on_message(std::forward<decltype(args)>(args)...); });
}

void TabColours::apply() {}

BOOL TabColours::on_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_INITDIALOG: {
        m_wnd = wnd;
        m_wnd_colours_mode = GetDlgItem(wnd, IDC_COLOURS_MODE);
        m_wnd_colours_element = GetDlgItem(wnd, IDC_COLOURS_ELEMENT);

        constexpr auto y_start = 94;
        constexpr auto y_spacing = 23;
        constexpr auto x_col_1 = 140;
        constexpr auto x_col_2 = 245;

        g_fill_text.create(wnd, uih::WindowPosition{x_col_1, y_start, 18, 14}, nullptr, true);
        g_fill_selection_text.create(wnd, uih::WindowPosition{x_col_1, y_start + y_spacing, 18, 14}, nullptr, true);
        g_fill_selection_text_inactive.create(
            wnd, uih::WindowPosition{x_col_1, y_start + y_spacing * 2, 18, 14}, nullptr, true);

        g_fill_active_item_frame.create(wnd, uih::WindowPosition{x_col_1, y_start + 92, 18, 14}, nullptr, true);

        g_fill_background.create(wnd, uih::WindowPosition{x_col_2, y_start, 18, 14}, nullptr, true);
        g_fill_selection_background.create(
            wnd, uih::WindowPosition{x_col_2, y_start + y_spacing, 18, 14}, nullptr, true);
        g_fill_selection_background_inactive.create(
            wnd, uih::WindowPosition{x_col_2, y_start + y_spacing * 2, 18, 14}, nullptr, true);

        ComboBox_AddString(m_wnd_colours_element, L"Global");
        ColoursClientList::g_get_list(m_colours_client_list);
        t_size count = m_colours_client_list.get_count();
        for (t_size i = 0; i < count; i++)
            ComboBox_AddString(
                m_wnd_colours_element, pfc::stringcvt::string_os_from_utf8(m_colours_client_list[i].m_name));

        ComboBox_SetCurSel(m_wnd_colours_element, 0);

        g_colours_manager_data.find_by_guid(m_element_guid, m_element_ptr);

        update_mode_combobox();
        update_fills();
        update_buttons();

        refresh_me(wnd);
    } break;
    case WM_DESTROY: {
        m_colours_client_list.remove_all();
        m_element_guid = pfc::guid_null;
        m_element_ptr.reset();
        m_element_api.release();
        m_wnd = nullptr;
    } break;
    case WM_COMMAND:
        switch (wp) {
        case IDC_COLOURS_ELEMENT | (CBN_SELCHANGE << 16): {
            int idx = ComboBox_GetCurSel((HWND)lp);
            m_element_api.release();
            if (idx != -1) {
                if (idx == 0)
                    m_element_guid = pfc::guid_null;
                else {
                    m_element_guid = m_colours_client_list[idx - 1].m_guid;
                    m_element_api = m_colours_client_list[idx - 1].m_ptr;
                }
                g_colours_manager_data.find_by_guid(m_element_guid, m_element_ptr);
            }
            update_fills();
            update_buttons();
            update_mode_combobox();
            return 0;
        }
        case IDC_COLOURS_MODE | (CBN_SELCHANGE << 16): {
            int idx = ComboBox_GetCurSel((HWND)lp);
            m_element_ptr->colour_mode = (cui::colours::colour_mode_t)ComboBox_GetItemData((HWND)lp, idx);
            update_fills();
            update_buttons();
            on_colour_changed();
            return 0;
        }
        case IDC_CHANGE_TEXT_BACK:
        case IDC_CHANGE_FRAME:
        case IDC_CHANGE_TEXT_FORE:
        case IDC_CHANGE_SELBACK:
        case IDC_CHANGE_SEL_FORE:
        case IDC_CHANGE_SEL_INACTIVE_BACK:
        case IDC_CHANGE_SEL_INACTIVE_FORE:
        case IDC_CUSTOM_FRAME:
            bool b_changed = false;
            if (wp == IDC_CHANGE_TEXT_BACK)
                b_changed = colour_picker(wnd, m_element_ptr->background, get_default_colour(colours::COLOUR_BACK));
            if (wp == IDC_CHANGE_TEXT_FORE)
                b_changed = colour_picker(wnd, m_element_ptr->text, get_default_colour(colours::COLOUR_TEXT));
            if (wp == IDC_CHANGE_SELBACK)
                b_changed = colour_picker(
                    wnd, m_element_ptr->selection_background, get_default_colour(colours::COLOUR_SELECTED_BACK));
            if (wp == IDC_CHANGE_SEL_FORE)
                b_changed = colour_picker(
                    wnd, m_element_ptr->selection_text, get_default_colour(colours::COLOUR_SELECTED_TEXT));
            if (wp == IDC_CHANGE_SEL_INACTIVE_BACK)
                b_changed = colour_picker(wnd, m_element_ptr->inactive_selection_background,
                    get_default_colour(colours::COLOUR_SELECTED_BACK_NO_FOCUS));
            if (wp == IDC_CHANGE_SEL_INACTIVE_FORE)
                b_changed = colour_picker(wnd, m_element_ptr->inactive_selection_text,
                    get_default_colour(colours::COLOUR_SELECTED_TEXT_NO_FOCUS));
            if (wp == IDC_CHANGE_FRAME)
                b_changed
                    = colour_picker(wnd, m_element_ptr->active_item_frame, get_default_colour(colours::COLOUR_FRAME));
            if (wp == IDC_CUSTOM_FRAME) {
                b_changed = true;
                m_element_ptr->use_custom_active_item_frame = (Button_GetCheck((HWND)lp) == BST_CHECKED);
            }
            if (b_changed) {
                update_fills();
                on_colour_changed();
                // apply();
            }
            if (wp == IDC_CUSTOM_FRAME)
                update_buttons();
            return 0;
        }
        break;
    }
    return 0;
}

void TabColours::on_colour_changed()
{
    if (m_element_api.is_valid())
        m_element_api->on_colour_changed(cui::colours::colour_flag_all);
    else if (m_element_guid == pfc::guid_null) {
        g_colours_manager_data.g_on_common_colour_changed(cui::colours::colour_flag_all);
        t_size count = m_colours_client_list.get_count();
        for (t_size i = 0; i < count; i++) {
            ColoursManagerData::entry_ptr_t p_data;
            g_colours_manager_data.find_by_guid(m_colours_client_list[i].m_guid, p_data);
            if (p_data->colour_mode == cui::colours::colour_mode_global)
                m_colours_client_list[i].m_ptr->on_colour_changed(cui::colours::colour_flag_all);
        }
    }
}

void TabColours::update_mode_combobox()
{
    ComboBox_ResetContent(m_wnd_colours_mode);
    t_size index;
    if (m_element_guid != pfc::guid_null) {
        index = ComboBox_AddString(m_wnd_colours_mode, L"Global");
        ComboBox_SetItemData(m_wnd_colours_mode, index, cui::colours::colour_mode_global);
    }
    index = ComboBox_AddString(m_wnd_colours_mode, L"System");
    ComboBox_SetItemData(m_wnd_colours_mode, index, cui::colours::colour_mode_system);
    if (!m_element_api.is_valid() || m_element_api->get_themes_supported()) {
        index = ComboBox_AddString(m_wnd_colours_mode, L"Themed");
        ComboBox_SetItemData(m_wnd_colours_mode, index, cui::colours::colour_mode_themed);
    }
    index = ComboBox_AddString(m_wnd_colours_mode, L"Custom");
    ComboBox_SetItemData(m_wnd_colours_mode, index, cui::colours::colour_mode_custom);

    ComboBox_SetCurSel(
        m_wnd_colours_mode, uih::combo_box_find_item_by_data(m_wnd_colours_mode, m_element_ptr->colour_mode));
}

void TabColours::update_buttons()
{
    EnableWindow(GetDlgItem(m_wnd, IDC_CHANGE_TEXT_BACK), get_change_colour_enabled(cui::colours::colour_background));
    EnableWindow(GetDlgItem(m_wnd, IDC_CHANGE_TEXT_FORE), get_change_colour_enabled(cui::colours::colour_text));
    EnableWindow(
        GetDlgItem(m_wnd, IDC_CHANGE_SELBACK), get_change_colour_enabled(cui::colours::colour_selection_background));
    EnableWindow(
        GetDlgItem(m_wnd, IDC_CHANGE_SEL_FORE), get_change_colour_enabled(cui::colours::colour_selection_text));
    EnableWindow(GetDlgItem(m_wnd, IDC_CHANGE_SEL_INACTIVE_BACK),
        get_change_colour_enabled(cui::colours::colour_inactive_selection_background));
    EnableWindow(GetDlgItem(m_wnd, IDC_CHANGE_SEL_INACTIVE_FORE),
        get_change_colour_enabled(cui::colours::colour_inactive_selection_text));
    EnableWindow(
        GetDlgItem(m_wnd, IDC_CHANGE_FRAME), get_change_colour_enabled(cui::colours::colour_active_item_frame));

    EnableWindow(g_fill_background.get_wnd(), get_colour_patch_enabled(cui::colours::colour_background));
    EnableWindow(g_fill_text.get_wnd(), get_colour_patch_enabled(cui::colours::colour_text));
    EnableWindow(
        g_fill_selection_background.get_wnd(), get_colour_patch_enabled(cui::colours::colour_selection_background));
    EnableWindow(g_fill_selection_text.get_wnd(), get_colour_patch_enabled(cui::colours::colour_selection_text));
    EnableWindow(g_fill_selection_background_inactive.get_wnd(),
        get_colour_patch_enabled(cui::colours::colour_inactive_selection_background));
    EnableWindow(g_fill_selection_text_inactive.get_wnd(),
        get_colour_patch_enabled(cui::colours::colour_inactive_selection_text));

    EnableWindow(g_fill_active_item_frame.get_wnd(), get_colour_patch_enabled(cui::colours::colour_active_item_frame));
    EnableWindow(GetDlgItem(m_wnd, IDC_CUSTOM_FRAME),
        m_element_ptr->colour_mode != cui::colours::colour_mode_global
            && (!m_element_api.is_valid()
                || (m_element_api->get_supported_bools() & cui::colours::bool_flag_use_custom_active_item_frame)));
}

bool TabColours::get_colour_patch_enabled(cui::colours::colour_identifier_t p_identifier)
{
    cui::colours::helper colour_helper(m_element_guid);

    if (p_identifier == cui::colours::colour_active_item_frame) {
        if (!m_element_api.is_valid()) {
            return m_element_ptr->colour_mode == cui::colours::colour_mode_custom
                || colour_helper.get_bool(cui::colours::bool_use_custom_active_item_frame);
        }
        bool is_colour_supported = (m_element_api->get_supported_colours() & (1 << p_identifier)) != 0;
        bool use_custom_frame_supported
            = (m_element_api->get_supported_bools() & (1 << cui::colours::bool_flag_use_custom_active_item_frame)) != 0;

        return is_colour_supported
            && (!use_custom_frame_supported || colour_helper.get_bool(cui::colours::bool_use_custom_active_item_frame));
    }
    return !m_element_api.is_valid() || (m_element_api->get_supported_colours() & (1 << p_identifier));
}

bool TabColours::get_change_colour_enabled(cui::colours::colour_identifier_t p_identifier)
{
    if (p_identifier == cui::colours::colour_active_item_frame)
        return m_element_ptr->colour_mode == cui::colours::colour_mode_custom
            && (!m_element_api.is_valid() || m_element_api->get_supported_colours() & (1 << p_identifier))
            && ((!m_element_api.is_valid()
                    || !(m_element_api->get_supported_bools() & cui::colours::bool_flag_use_custom_active_item_frame))
                || cui::colours::helper(m_element_guid).get_bool(cui::colours::bool_use_custom_active_item_frame));
    return (m_element_ptr->colour_mode == cui::colours::colour_mode_custom
        && (!m_element_api.is_valid() || (m_element_api->get_supported_colours() & (1 << p_identifier))));
}

void TabColours::update_fills()
{
    cui::colours::helper p_manager(m_element_guid);
    if (p_manager.get_themed() && (!m_element_api.is_valid() || m_element_api->get_themes_supported())) {
        g_fill_selection_background.set_fill_themed(
            L"ListView", LVP_LISTITEM, LISS_SELECTED, p_manager.get_colour(cui::colours::colour_selection_background));
        g_fill_selection_background_inactive.set_fill_themed(L"ListView", LVP_LISTITEM, LISS_SELECTEDNOTFOCUS,
            p_manager.get_colour(cui::colours::colour_inactive_selection_background));
        g_fill_selection_text_inactive.set_fill_themed_colour(L"ListView", COLOR_BTNTEXT);
        g_fill_selection_text.set_fill_themed_colour(L"ListView", COLOR_BTNTEXT);
    } else {
        g_fill_selection_text.set_fill_colour(p_manager.get_colour(cui::colours::colour_selection_text));
        g_fill_selection_background.set_fill_colour(p_manager.get_colour(cui::colours::colour_selection_background));
        g_fill_selection_background_inactive.set_fill_colour(
            p_manager.get_colour(cui::colours::colour_inactive_selection_background));
        g_fill_selection_text_inactive.set_fill_colour(
            p_manager.get_colour(cui::colours::colour_inactive_selection_text));
    }
    g_fill_text.set_fill_colour(p_manager.get_colour(cui::colours::colour_text));
    g_fill_background.set_fill_colour(p_manager.get_colour(cui::colours::colour_background));
    g_fill_active_item_frame.set_fill_colour(p_manager.get_colour(cui::colours::colour_active_item_frame));

    Button_SetCheck(
        GetDlgItem(m_wnd, IDC_CUSTOM_FRAME), p_manager.get_bool(cui::colours::bool_use_custom_active_item_frame));
}

void TabColours::refresh_me(HWND wnd)
{
    initialising = true;
    initialising = false;
}
