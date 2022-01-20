#pragma once

#include "menu_helpers.h"

namespace cui::config {

constexpr GUID advconfig_branch_columns_ui_id{
    0xd2dd7ffc, 0xf780, 0x4fa3, {0x89, 0x52, 0x38, 0xd8, 0x2c, 0x8c, 0x1e, 0x4b}};
constexpr GUID advconfig_branch_notification_area_id{
    0x8b39648d, 0x3e4, 0x4748, {0x8d, 0xd3, 0x71, 0x6b, 0x76, 0x8a, 0x39, 0xf0}};

extern advconfig_checkbox_factory advbool_notification_icon_x_buttons;
extern advconfig_checkbox_factory advbool_close_to_notification_icon;

extern cfg_bool cfg_playlist_tabs_middle_click;
} // namespace cui::config

class ConfigWindowPlacement : public cfg_struct_t<WINDOWPLACEMENT> {
    using cfg_struct_t<WINDOWPLACEMENT>::operator=;
    void get_data_raw(stream_writer* out, abort_callback& p_abort) override;

public:
    ConfigWindowPlacement(const GUID& p_guid);
};

class ConfigMenuItem : public cfg_struct_t<MenuItemIdentifier> {
public:
    using cfg_struct_t<MenuItemIdentifier>::operator=;
    using cfg_struct_t<MenuItemIdentifier>::operator MenuItemIdentifier;
    void reset()
    {
        MenuItemIdentifier temp;
        *this = temp;
    }
    explicit ConfigMenuItem(const GUID& p_guid, const MenuItemIdentifier& p_val)
        : cfg_struct_t<MenuItemIdentifier>(p_guid, p_val)
    {
    }
    explicit ConfigMenuItem(const GUID& p_guid, const GUID& p_val, const GUID& psub = pfc::guid_null)
        : cfg_struct_t<MenuItemIdentifier>(p_guid, MenuItemIdentifier{p_val, psub})
    {
    }
    explicit ConfigMenuItem(const GUID& p_guid) : cfg_struct_t<MenuItemIdentifier>(p_guid, MenuItemIdentifier{}) {}
};

namespace settings {
extern cfg_bool show_status_pane;

extern fbh::ConfigObjectBoolFactory<> allow_locked_panel_resizing;
extern fbh::ConfigUint32DpiAware custom_splitter_divider_width;

extern fbh::ConfigInt32DpiAware playlist_switcher_item_padding, playlist_view_item_padding;
} // namespace settings

extern cfg_string cfg_playlist_switcher_tagz;
extern ConfigMenuItem cfg_playlist_double;

extern cfg_int cfg_global, cfg_cur_prefs_col, cfg_header_hottrack, cfg_sortsel, cfg_global_sort, cfg_vis, cfg_vis2,
    cfg_vis_edge, cfg_lock, cfg_header, cfg_drop_at_end, cfg_mclick, cfg_mclick2, cfg_balloon, cfg_scroll_h_no_v,
    cfg_ellipsis, cfg_tabs_multiline, cfg_frame, cfg_show_seltime, cfg_plistframe, cfg_tooltips_clipped, cfg_np,
    cfg_show_systray, cfg_minimise_to_tray, cfg_show_vol, cfg_custom_icon, cfg_custom_buttons, cfg_drag_autoswitch,
    cfg_plist_width, cfg_drag_pl, cfg_pl_autohide, cfg_sel_dp, cfg_alternative_sel, cfg_plm_rename, cfg_pgen_playlist,
    cfg_pgen_tf, cfg_autoswitch_delay, cfg_pgen_dir, cfg_custom_buttons_over, cfg_custom_buttons_transparency,
    cfg_replace_drop_underscores, cfg_status, cfg_show_sort_arrows, cfg_toolbar_disable_default_drawing,
    cfg_sidebar_use_custom_show_delay, cfg_sidebar_show_delay, cfg_sidebar_hide_delay, cfg_toolbars,
    cfg_playlist_switcher_use_tagz, cfg_playlist_middle_action, cfg_playlist_panel_delete, cfg_nohscroll;

extern cfg_bool cfg_go_to_tray;

extern cfg_string cfg_tray_icon_path, cfg_export, cfg_import, cfg_custom_buttons_path, cfg_globalstring, cfg_colour,
    cfg_pgenstring;

extern ConfigMenuItem cfg_statusdbl;

extern ConfigWindowPlacement cfg_window_placement_columns;
