#pragma once

#include "config_appearance.h"
#include "config.h"

class TabColours : public PreferencesTab {
    HWND m_wnd{nullptr};
    HWND m_wnd_colours_mode{nullptr};
    HWND m_wnd_colours_element{nullptr};
    uih::FillWindow g_fill_text;
    uih::FillWindow g_fill_background;
    uih::FillWindow g_fill_selection_text;
    uih::FillWindow g_fill_selection_background;
    uih::FillWindow g_fill_selection_text_inactive;
    uih::FillWindow g_fill_selection_background_inactive;
    uih::FillWindow g_fill_active_item_frame;
    GUID m_element_guid{};
    ColourManagerData::entry_ptr_t m_element_ptr;
    cui::colours::client::ptr m_element_api;
    ColoursClientList m_colours_client_list;

public:
    void refresh_me(HWND wnd);

    void update_fills();
    bool get_change_colour_enabled(cui::colours::colour_identifier_t p_identifier);
    bool get_colour_patch_enabled(cui::colours::colour_identifier_t p_identifier);
    void update_buttons();
    void update_mode_combobox();

    void on_colour_changed();

    BOOL on_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);
    void apply();
    HWND create(HWND wnd) override;
    const char* get_name() override;
    bool get_help_url(pfc::string_base& p_out) override;
    bool is_active();

private:
    bool initialising{false};
    cui::prefs::PreferencesTabHelper m_helper{IDC_TITLE1};
};

extern TabColours g_tab_appearance;
