#pragma once

#include "core_dark_list_view.h"
#include "ng_playlist_style.h"
#include "../config.h"
#include "../list_view_panel.h"

namespace cui::panels::playlist_view {

extern const GUID g_guid_items_font, g_guid_header_font, g_guid_group_header_font;

extern cfg_bool cfg_artwork_reflection;
extern fbh::ConfigUint32DpiAware cfg_artwork_width;
extern fbh::ConfigBool cfg_grouping;
extern fbh::ConfigBool cfg_indent_groups;
extern fbh::ConfigBool cfg_use_custom_group_indentation_amount;
extern fbh::ConfigInt32DpiAware cfg_custom_group_indentation_amount;
extern fbh::ConfigBool cfg_show_artwork;

wil::unique_hbitmap g_create_hbitmap_from_image(
    Gdiplus::Bitmap& bm, int& cx, int& cy, COLORREF cr_back, bool b_reflection);
wil::unique_hbitmap g_create_hbitmap_from_data(
    const album_art_data_ptr& data, int& cx, int& cy, COLORREF cr_back, bool b_reflection);
bool g_get_default_nocover_bitmap_data(album_art_data_ptr& p_out, abort_callback& p_abort);
wil::unique_hbitmap g_get_nocover_bitmap(int cx, int cy, COLORREF cr_back, bool b_reflection, abort_callback& p_abort);
void set_font_size(float point_delta);

struct PlaylistCacheItem {
    std::optional<GUID> playlist_id;
    std::optional<uih::lv::SavedScrollPosition> saved_scroll_position;
};

template <class item_t>
class PlaylistCache {
public:
    void initialise(const std::unordered_map<GUID, uih::lv::SavedScrollPosition>& initial_data)
    {
        const auto api = playlist_manager::get();
        const auto playlist_count = api->get_playlist_count();
        m_items.set_size(playlist_count);

        playlist_manager_v5::ptr api_v5;
        if (api->service_query_t(api_v5)) {
            for (const auto playlist_index : std::ranges::views::iota(size_t{}, playlist_count)) {
                auto playlist_id = api_v5->playlist_get_guid(playlist_index);
                m_items[playlist_index].playlist_id = playlist_id;

                const auto iter = initial_data.find(playlist_id);
                if (iter != initial_data.end()) {
                    m_items[playlist_index].saved_scroll_position = iter->second;
                }
            }
        }

        m_callback = std::make_unique<PlaylistCallback>(this);
    }
    void deinitialise() { m_callback.reset(); }

    void set_item(size_t index, std::optional<uih::lv::SavedScrollPosition> saved_scroll_position)
    {
        m_items[index].saved_scroll_position = std::move(saved_scroll_position);
    }
    const PlaylistCacheItem& get_item(size_t index) const { return m_items[index]; }
    auto size() const { return m_items.get_size(); }

    auto begin() const { return m_items.begin(); }
    auto end() const { return m_items.end(); }

private:
    class PlaylistCallback : public playlist_callback_impl_base {
    public:
        PlaylistCallback(PlaylistCache* owner)
            : playlist_callback_impl_base(
                  flag_on_playlist_created | flag_on_playlists_reorder | flag_on_playlists_removed)
            , m_owner{owner}
        {
        }

    private:
        void on_playlist_created(size_t p_index, const char* p_name, size_t p_name_len) noexcept override
        {
            m_owner->on_playlist_created(p_index, {p_name, p_name_len});
        }
        void on_playlists_reorder(const size_t* p_order, size_t p_count) noexcept override
        {
            m_owner->on_playlists_reorder({p_order, p_count});
        }
        void on_playlists_removed(const bit_array& p_mask, size_t p_old_count, size_t p_new_count) noexcept override
        {
            m_owner->on_playlists_removed(p_mask, p_old_count, p_new_count);
        }

        PlaylistCache* m_owner{};
    };

    void on_playlist_created(size_t index, std::string_view name)
    {
        std::optional<GUID> playlist_id;
        playlist_manager_v5::ptr api;
        if (playlist_manager_v5::tryGet(api)) {
            playlist_id = api->playlist_get_guid(index);
        }
        m_items.insert_item({playlist_id, {}}, index);
    }
    void on_playlists_reorder(std::span<const size_t> order) { m_items.reorder(order.data()); }
    void on_playlists_removed(const bit_array& mask, size_t old_count, size_t new_count) { m_items.remove_mask(mask); }

    pfc::list_t<PlaylistCacheItem> m_items;
    std::unique_ptr<PlaylistCallback> m_callback;
};

class ColumnData {
public:
    titleformat_object::ptr m_display_script;
    titleformat_object::ptr m_style_script;
    titleformat_object::ptr m_sort_script;
};

class BaseArtworkCompletionNotify {
public:
    using ptr_t = std::shared_ptr<BaseArtworkCompletionNotify>;

    virtual ~BaseArtworkCompletionNotify() = default;
    virtual void on_completion(const std::shared_ptr<class ArtworkReader>& p_reader) = 0;

private:
};

class ArtworkReader : public mmh::Thread {
public:
    bool is_aborting() { return m_abort.is_aborting(); }
    void abort() { m_abort.abort(); }

    // only called when thread closed
    bool did_succeed() { return m_succeeded; }
    bool is_ready() { return !is_thread_open(); }
    const std::unordered_map<GUID, wil::shared_hbitmap>& get_content() const { return m_bitmaps; }

    void initialise(const metadb_handle_ptr& p_handle, int cx, int cy, COLORREF cr_back, bool b_reflection,
        BaseArtworkCompletionNotify::ptr_t p_notify, std::shared_ptr<class ArtworkReaderManager> p_manager)
    {
        m_handle = p_handle;
        m_notify = std::move(p_notify);
        m_cx = cx;
        m_cy = cy;
        m_reflection = b_reflection;
        m_back = cr_back;
        m_manager = std::move(p_manager);
    }
    void send_completion_notification(const std::shared_ptr<ArtworkReader>& p_this)
    {
        if (m_notify) {
            m_notify->on_completion(p_this);
        }
    }

protected:
    DWORD on_thread() override;

private:
    unsigned read_artwork(abort_callback& p_abort);

    std::unordered_map<GUID, wil::shared_hbitmap> m_bitmaps;
    int m_cx{0};
    int m_cy{0};
    COLORREF m_back{RGB(255, 255, 255)};
    bool m_reflection{false};
    metadb_handle_ptr m_handle;
    BaseArtworkCompletionNotify::ptr_t m_notify;
    bool m_succeeded{false};
    abort_callback_impl m_abort;
    std::shared_ptr<class ArtworkReaderManager> m_manager;
};

class ArtworkReaderManager : public std::enable_shared_from_this<ArtworkReaderManager> {
public:
    void abort_task(size_t index)
    {
        {
            if (m_current_readers[index]->is_thread_open()) {
                m_current_readers[index]->abort();
                m_aborting_readers.add_item(m_current_readers[index]);
            }
            m_current_readers.remove_by_idx(index);
        }
    }
    void abort_current_tasks()
    {
        m_pending_readers.remove_all();
        size_t i = m_current_readers.get_count();
        for (; i; i--)
            abort_task(i - 1);
    }

    void reset() { abort_current_tasks(); }

    enum {
        max_readers = 4
    };

    void request(const metadb_handle_ptr& p_handle, std::shared_ptr<ArtworkReader>& p_out, int cx, int cy,
        COLORREF cr_back, bool b_reflection, BaseArtworkCompletionNotify::ptr_t p_notify);

    void flush_pending()
    {
        size_t count = m_current_readers.get_count();
        size_t count_pending = m_pending_readers.get_count();
        if (count < max_readers) {
            if (count_pending) {
                std::shared_ptr<ArtworkReader> p_reader = m_pending_readers[count_pending - 1];
                m_pending_readers.remove_by_idx(count_pending - 1);
                p_reader->set_priority(THREAD_PRIORITY_BELOW_NORMAL);
                p_reader->create_thread();
                m_current_readers.add_item(p_reader);
            }
        }
    }

    void initialise() {}

    void deinitialise()
    {
        m_pending_readers.remove_all();

        size_t i = m_aborting_readers.get_count();
        for (; i; i--) {
            m_aborting_readers[i - 1]->wait_for_and_release_thread();
            m_aborting_readers.remove_by_idx(i - 1);
        }
        i = m_current_readers.get_count();
        for (; i; i--) {
            m_current_readers[i - 1]->wait_for_and_release_thread();
            m_current_readers.remove_by_idx(i - 1);
        }

        {
            insync(m_nocover_sync);
            m_nocover_bitmap.reset();
        }
    }

    void on_reader_completion(const ArtworkReader* ptr);
    void on_reader_abort(const ArtworkReader* ptr);

    ArtworkReaderManager() = default;

    wil::shared_hbitmap request_nocover_image(
        int cx, int cy, COLORREF cr_back, bool b_reflection, abort_callback& p_abort);
    void flush_nocover() { m_nocover_bitmap.reset(); }

private:
    bool find_aborting_reader(const ArtworkReader* ptr, size_t& index)
    {
        size_t count = m_aborting_readers.get_count();
        for (size_t i = 0; i < count; i++)
            if (&*m_aborting_readers[i] == ptr) {
                index = i;
                return true;
            }
        return false;
    }
    bool find_current_reader(const ArtworkReader* ptr, size_t& index)
    {
        size_t count = m_current_readers.get_count();
        for (size_t i = 0; i < count; i++)
            if (&*m_current_readers[i] == ptr) {
                index = i;
                return true;
            }
        return false;
    }
    pfc::list_t<std::shared_ptr<ArtworkReader>> m_aborting_readers;
    pfc::list_t<std::shared_ptr<ArtworkReader>> m_current_readers;
    pfc::list_t<std::shared_ptr<ArtworkReader>> m_pending_readers;

    critical_section m_nocover_sync;
    wil::shared_hbitmap m_nocover_bitmap;
    size_t m_nocover_cx{0}, m_nocover_cy{0};
};

class ColoursClient : public colours::client {
public:
    static constexpr GUID id{0xc882d3ac, 0xc014, 0x44df, {0x9c, 0x7e, 0x2d, 0xad, 0xf3, 0x76, 0x45, 0xa0}};

    const GUID& get_client_guid() const override { return id; }

    void get_name(pfc::string_base& p_out) const override { p_out = "Playlist view"; }

    uint32_t get_supported_colours() const override { return colours::colour_flag_all; } // bit-mask
    uint32_t get_supported_bools() const override
    {
        return colours::bool_flag_use_custom_active_item_frame | colours::bool_flag_dark_mode_enabled;
    } // bit-mask
    bool get_themes_supported() const override { return true; }

    void on_colour_changed(uint32_t mask) const override;

    void on_bool_changed(uint32_t mask) const override;
};

class PlaylistViewRenderer : public uih::lv::DefaultRenderer {
public:
    explicit PlaylistViewRenderer(class PlaylistView* playlist_view) : m_playlist_view{playlist_view} {}

    void render_group_info(uih::lv::RendererContext context, size_t index, RECT rc) override;

    void render_group(uih::lv::RendererContext context, size_t item_index, size_t group_index, std::string_view text,
        int indentation, size_t level, RECT rc) override;

    void render_item(uih::lv::RendererContext context, size_t index, std::vector<uih::lv::RendererSubItem> sub_items,
        int indentation, bool b_selected, bool b_window_focused, bool b_highlight, bool should_hide_focus,
        bool b_focused, RECT rc) override;

    class PlaylistView* m_playlist_view;
};

class PlaylistView
    : public ListViewPanelBase<ColoursClient, uie::playlist_window>
    , playlist_callback {
    friend class NgTfThread;
    friend class PlaylistViewRenderer;

public:
    PlaylistView();
    ~PlaylistView();

    static std::vector<PlaylistView*> g_windows;
    inline static std::unique_ptr<uie::container_window_v3> s_message_window;

    static void g_on_groups_change();
    static void s_on_indent_groups_change();
    static void s_on_group_indentation_amount_change();
    static void g_on_columns_change();
    static void g_on_column_widths_change(const PlaylistView* p_skip = nullptr);
    static void s_update_all_items();
    static void g_on_autosize_change();
    static void g_on_show_artwork_change();
    static void g_on_alternate_selection_change();
    static void g_on_artwork_width_change(const PlaylistView* p_skip = nullptr);
    static void s_flush_artwork(bool b_redraw = false, const PlaylistView* p_skip = nullptr);
    static void g_on_vertical_item_padding_change();
    static void g_on_show_header_change();
    static void g_on_playback_follows_cursor_change(bool b_val);
    static void g_on_show_tooltips_change();
    static void s_on_dark_mode_status_change();
    static void g_on_font_change();
    static void g_on_header_font_change();
    static void g_on_group_header_font_change();
    static void g_on_sorting_enabled_change();
    static void g_on_show_sort_indicators_change();
    static void g_on_edge_style_change();
    static void s_redraw_all();
    static void g_on_time_change();

    const GUID& get_extension_guid() const override;
    void get_name(pfc::string_base& out) const override;
    bool get_short_name(pfc::string_base& out) const override;
    void get_category(pfc::string_base& out) const override;
    unsigned get_type() const override;

    void get_config(stream_writer* p_writer, abort_callback& p_abort) const override;
    void set_config(stream_reader* p_reader, size_t p_size, abort_callback& p_abort) override;

    bool m_dragging{false};
    wil::com_ptr_t<IDataObject> m_DataObject;
    size_t m_dragging_initial_playlist;

protected:
    using InsertItemsContainer = pfc::array_t<InsertItem>;

private:
    static const GUID g_extension_guid;
    enum {
        timer_date_change = TIMER_BASE
    };

    class PlaylistViewGroup : public Group {
    public:
        using self_t = PlaylistViewGroup;
        using ptr = pfc::refcounted_object_ptr_t<self_t>;
        SharedCellStyleData::ptr m_style_data;

        bool m_artwork_load_attempted{false};
        bool m_artwork_load_succeeded{false};
        // bool m_data_to_bitmap_attempted;
        // album_art_data_ptr m_artwork_data;
        wil::shared_hbitmap m_artwork_bitmap; // cached for display

        PlaylistViewGroup() = default;
    };

    class PlaylistViewItem : public Item {
    public:
        using self_t = PlaylistViewItem;
        using ptr = pfc::refcounted_object_ptr_t<self_t>;
        style_data_t m_style_data;
        PlaylistViewGroup* get_group(size_t index)
        {
            return static_cast<PlaylistViewGroup*>(m_groups[index].get_ptr());
        }
        size_t get_group_count() { return m_groups.size(); }
    };

    class PlaylistViewSearchContext : public ListViewSearchContextBase {
    public:
        explicit PlaylistViewSearchContext(titleformat_object::ptr global_script, titleformat_object::ptr column_script)
            : m_global_script(std::move(global_script))
            , m_column_script(std::move(column_script))
        {
        }

        const char* get_item_text(size_t index) override;

    private:
        SYSTEMTIME m_systemtime;
        metadb_handle_list m_tracks;
        std::vector<std::optional<std::string>> m_items;
        std::optional<size_t> m_start_index;
        playlist_manager_v4::ptr m_playlist_manager{playlist_manager_v4::get()};
        metadb_v2::ptr m_metadb{metadb_v2::get()};
        titleformat_object::ptr m_global_script{};
        titleformat_object::ptr m_column_script{};
    };

    static void s_create_message_window();
    static void s_destroy_message_window();
    static auto get_artwork_left_right_padding() { return cfg_indent_groups ? 0 : 5_spx; }

    virtual void flush_artwork_images()
    {
        if (m_artwork_manager) {
            m_artwork_manager->abort_current_tasks();
            m_artwork_manager->flush_nocover();
        }
        size_t count = get_item_count();
        for (size_t i = 0; i < count; i++) {
            size_t cg = get_item(i)->get_group_count();
            if (cg) {
                PlaylistViewGroup* ptr = get_item(i)->get_group(cg - 1);
                // ptr->m_data_to_bitmap_attempted = false;
                ptr->m_artwork_load_succeeded = false;
                ptr->m_artwork_load_attempted = false;
                ptr->m_artwork_bitmap.reset();
            }
        }
    }

    void notify_on_group_info_area_size_change(int new_width) override
    {
        cfg_artwork_width = new_width;
        // flush_artwork_images();
        g_on_artwork_width_change(nullptr /*this*/);
    }
    void on_artwork_read_complete(const PlaylistViewGroup::ptr& p_group, const std::shared_ptr<ArtworkReader>& p_reader)
    {
        if (!p_reader->is_aborting()) {
            size_t count = get_item_count();
            size_t group_count = m_scripts.get_count();

            if (group_count) {
                for (size_t i = 0; i < count; i++) {
                    auto* item = static_cast<PlaylistViewItem*>(get_item(i));
                    if (item->get_group(group_count - 1) == p_group.get_ptr()) {
                        auto&& content = p_reader->get_content();
                        auto content_iter = content.find(album_art_ids::cover_front);

                        if (content_iter != content.end()) {
                            p_group->m_artwork_bitmap = content_iter->second;
                            p_group->m_artwork_load_succeeded = true;
                            invalidate_item_group_info_area(i);
                        }
                        break;
                    }
                }
            }
        }
    }

    class ArtworkCompletionNotify : public BaseArtworkCompletionNotify {
    public:
        using ptr_t = std::shared_ptr<ArtworkCompletionNotify>;

        void on_completion(const std::shared_ptr<ArtworkReader>& p_reader) noexcept override
        {
            m_window->on_artwork_read_complete(m_group, p_reader);
        }

        PlaylistViewGroup::ptr m_group;
        service_ptr_t<PlaylistView> m_window;
    };

    wil::shared_hbitmap request_group_artwork(size_t index_item);

    void update_all_items();
    void refresh_all_items_text();
    void update_items(size_t index, size_t count);

    Item* storage_create_item() override { return new PlaylistViewItem; }

    Group* storage_create_group() override { return new PlaylistViewGroup; }

    PlaylistViewItem* get_item(size_t index) { return static_cast<PlaylistViewItem*>(ListView::get_item(index)); }

    void notify_update_item_data(size_t index) override;

    std::unique_ptr<ListViewSearchContextBase> create_search_context() override
    {
        if (!static_api_test_t<metadb_v2>() || m_column_data.size() == 0) {
            return ListViewPanelBase::create_search_context();
        }

        return std::make_unique<PlaylistViewSearchContext>(m_script_global, m_column_data[0].m_display_script);
    }

    const style_data_t& get_style_data(size_t index);
    void get_insert_items(size_t start, size_t count, InsertItemsContainer& items);
    void flush_items();
    void reset_items();

    void on_items_added(size_t playlist, size_t start, const pfc::list_base_const_t<metadb_handle_ptr>& p_data,
        const bit_array& p_selection) noexcept override;
    void on_items_reordered(size_t playlist, const size_t* p_order, size_t p_count) noexcept override;
    void on_items_removed(
        size_t playlist, const bit_array& p_mask, size_t p_old_count, size_t p_new_count) noexcept override;
    void on_items_selection_change(
        size_t playlist, const bit_array& p_affected, const bit_array& p_state) noexcept override;
    void on_item_focus_change(size_t playlist, size_t p_from, size_t p_to) noexcept override;
    void on_items_modified(size_t playlist, const bit_array& p_mask) noexcept override;
    void on_items_modified_fromplayback(
        size_t playlist, const bit_array& p_mask, play_control::t_display_level p_level) noexcept override;
    void on_items_replaced(size_t playlist, const bit_array& p_mask,
        const pfc::list_base_const_t<t_on_items_replaced_entry>& p_data) noexcept override;
    void on_item_ensure_visible(size_t playlist, size_t p_idx) noexcept override;

    void on_playlist_activate(size_t p_old, size_t p_new) noexcept override;
    void on_playlist_renamed(size_t playlist, const char* p_new_name, size_t p_new_name_len) noexcept override;

    void on_items_removing(size_t playlist, const bit_array& p_mask, size_t p_old_count, size_t p_new_count) override {}
    void on_default_format_changed() override {}
    void on_playback_order_changed(size_t p_new_index) override {}
    void on_playlist_locked(size_t playlist, bool p_locked) override {}
    void on_playlist_created(size_t p_index, const char* p_name, size_t p_name_len) override {}
    void on_playlists_reorder(const size_t* p_order, size_t p_count) override {}
    void on_playlists_removing(const bit_array& p_mask, size_t p_old_count, size_t p_new_count) override {}
    void on_playlists_removed(const bit_array& p_mask, size_t p_old_count, size_t p_new_count) override {}

    void on_first_show() override;

    void populate_list(const std::optional<uih::lv::SavedScrollPosition>& scroll_position = std::nullopt);
    void insert_tracks(size_t index, const pfc::list_base_const_t<metadb_handle_ptr>& tracks,
        const std::optional<uih::lv::SavedScrollPosition>& scroll_position = std::nullopt);
    void refresh_groups(bool b_update_columns = false);
    void refresh_columns();
    void set_group_info_area_size();
    void on_groups_change();
    void on_artwork_width_change();
    void on_columns_change();
    void on_column_widths_change();
    size_t column_index_display_to_actual(size_t display_index);
    size_t column_index_actual_to_display(size_t actual_index);

    void notify_on_initialisation() override;
    void notify_on_create() override;
    void notify_on_destroy() override;
    void set_focus() override { SetFocus(get_wnd()); }

    size_t get_highlight_item() override;
    bool notify_on_contextmenu(const POINT& pt, bool from_keyboard) override;
    bool notify_on_contextmenu_header(const POINT& pt, const HDHITTESTINFO& ht) override;
    bool notify_on_middleclick(bool on_item, size_t index) override;
    bool notify_on_doubleleftclick_nowhere() override;

    bool notify_on_keyboard_keydown_filter(UINT msg, WPARAM wp, LPARAM lp) override;
    bool notify_on_keyboard_keydown_remove() override;

    bool notify_on_keyboard_keydown_search() override;

    bool notify_on_keyboard_keydown_undo() override;
    bool notify_on_keyboard_keydown_redo() override;
    bool notify_on_keyboard_keydown_cut() override;
    bool notify_on_keyboard_keydown_copy() override;
    bool notify_on_keyboard_keydown_paste() override;

    void notify_on_column_size_change(size_t index, int new_width) override
    {
        size_t act = column_index_display_to_actual(index);
        if (act != pfc_infinite && act < g_columns.get_count()) {
            g_columns[act]->width = new_width;
            g_on_column_widths_change(this);
        }
    }

    void notify_on_header_rearrange(size_t index_from, size_t index_to) override
    {
        size_t act_from = column_index_display_to_actual(index_from);
        size_t act_to = column_index_display_to_actual(index_to);
        g_columns.move(act_from, act_to);
        g_on_columns_change();
    }

    bool notify_on_timer(UINT_PTR timerid) override
    {
        if (timerid == timer_date_change) {
            update_items(0, get_item_count());
            return true;
        }
        return false;
    }

    void on_time_change()
    {
        update_items(0, get_item_count());
        set_day_timer();
    }

    bool m_day_timer_active{false};

    void kill_day_timer()
    {
        if (m_day_timer_active) {
            KillTimer(get_wnd(), timer_date_change);
            m_day_timer_active = false;
        }
    }

    void set_day_timer()
    {
        kill_day_timer();
        SYSTEMTIME st;
        GetLocalTime(&st);
        unsigned ms
            = /*24**/ 60 * 60 * 1000 - (st.wMilliseconds + ((/*st.wHour*60 + */ st.wMinute) * 60 + st.wSecond) * 1000);
        SetTimer(get_wnd(), timer_date_change, ms, nullptr);
        m_day_timer_active = true;
    }

    void notify_sort_column(size_t index, bool b_descending, bool b_selection_only) override;

    void sort_by_column_fb2k_v1(size_t column_index, bool b_descending, bool b_selection_only);
    void sort_by_column_fb2k_v2(size_t index, bool b_descending, bool b_selection_only);

    size_t storage_get_focus_item() override;
    void storage_set_focus_item(size_t index) override;
    void storage_get_selection_state(bit_array_var& out) override;
    bool storage_set_selection_state(
        const bit_array& p_affected, const bit_array& p_status, bit_array_var* p_changed = nullptr) override;
    bool storage_get_item_selected(size_t index) override;
    size_t storage_get_selection_count(size_t max) override;

    void execute_default_action(size_t index, size_t column, bool b_keyboard, bool b_ctrl) override;
    void move_selection(int delta) override;
    bool do_drag_drop(WPARAM wp) override;

    bool notify_before_create_inline_edit(
        const pfc::list_base_const_t<size_t>& indices, size_t column, bool b_source_mouse) override;
    bool notify_create_inline_edit(const pfc::list_base_const_t<size_t>& indices, size_t column,
        pfc::string_base& p_text, size_t& p_flags, wil::com_ptr<IUnknown>& autocomplete_entries) override;
    void notify_save_inline_edit(const char* value) override;
    void notify_exit_inline_edit() override;

    void notify_on_set_focus(HWND wnd_lost) override;
    void notify_on_kill_focus(HWND wnd_receiving) override;

    void notify_on_menu_select(WPARAM wp, LPARAM lp) override;

private:
    service_list_t<titleformat_object> m_scripts;
    pfc::list_t<ColumnData> m_column_data;
    pfc::array_t<bool> m_column_mask;
    pfc::list_t<pfc::string8> m_edit_fields;
    pfc::string8 m_edit_field;
    metadb_handle_list m_edit_handles;
    service_ptr_t<titleformat_object> m_script_global, m_script_global_style;
    service_ptr_t<playlist_manager_v4> m_playlist_api;
    bool m_ignore_callback{false};
    ULONG_PTR m_gdiplus_token{NULL};
    bool m_gdiplus_initialised{false};

    mainmenu_manager::ptr m_mainmenu_manager;
    contextmenu_manager::ptr m_contextmenu_manager;
    ui_status_text_override::ptr m_status_text_override;

    UINT m_mainmenu_manager_base{NULL};
    UINT m_contextmenu_manager_base{NULL};

    std::unordered_map<GUID, uih::lv::SavedScrollPosition> m_initial_scroll_positions;
    mutable PlaylistCache<PlaylistCacheItem> m_playlist_cache;

    std::shared_ptr<ArtworkReaderManager> m_artwork_manager;
    ui_selection_holder::ptr m_selection_holder;

    library_meta_autocomplete::ptr m_library_autocomplete_v1;
    library_meta_autocomplete_v2::ptr m_library_autocomplete_v2;
};

class PlaylistViewDropTarget : public IDropTarget {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID FAR* ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override;
    HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override;
    HRESULT STDMETHODCALLTYPE DragLeave() override;
    HRESULT STDMETHODCALLTYPE Drop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override;
    explicit PlaylistViewDropTarget(PlaylistView* playlist);

private:
    HRESULT UpdateDropDescription(IDataObject* pDataObj, DWORD pdwEffect);

    long drop_ref_count;
    bool last_rmb;
    bool m_is_accepted_type;
    service_ptr_t<PlaylistView> p_playlist;
    wil::com_ptr_t<IDataObject> m_DataObject;
    wil::com_ptr_t<IDropTargetHelper> m_DropTargetHelper;
};

class PlaylistViewDropSource : public IDropSource {
    long refcount;
    service_ptr_t<PlaylistView> p_playlist;
    DWORD m_initial_key_state;

public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState) override;
    HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD dwEffect) override;
    PlaylistViewDropSource(PlaylistView* playlist, DWORD initial_key_state);
};

class GroupsPreferencesTab : public PreferencesTab {
public:
    class GroupsListView : public helpers::CoreDarkListView {
    public:
        explicit GroupsListView(GroupsPreferencesTab* tab) : m_tab(*tab) {}

        void notify_on_initialisation() override
        {
            helpers::CoreDarkListView::notify_on_initialisation();

            set_selection_mode(SelectionMode::SingleRelaxed);
            set_show_header(false);
            set_columns({{"Group", 100}});
            set_autosize(true);
        }

    private:
        void execute_default_action(size_t index, size_t column, bool b_keyboard, bool b_ctrl) override
        {
            m_tab.on_group_default_action(index);
        }

        GroupsPreferencesTab& m_tab;
    };

    HWND create(HWND wnd) override
    {
        return m_helper.create(wnd, IDD_PREFS_PVIEW_GROUPS,
            [this](auto&&... args) { return ConfigProc(std::forward<decltype(args)>(args)...); });
    }
    const char* get_name() override { return "Grouping"; }
    bool get_help_url(pfc::string_base& p_out) override
    {
        p_out = "http://yuo.be/wiki/columns_ui:config:playlist_view:grouping";
        return true;
    }

private:
    BOOL ConfigProc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);
    void refresh_enabled_options() const;

    void on_group_default_action(size_t index);

    HWND m_wnd{};
    bool m_initialised{};
    GroupsListView m_groups_list_view{this};
    prefs::PreferencesTabHelper m_helper{{IDC_TITLE1}, {IDC_H2_TITLE}};
};
} // namespace cui::panels::playlist_view

namespace playlist_utils {
bool check_clipboard();
bool cut();
bool copy();
bool paste_at_focused_item(HWND wnd);
bool paste(HWND wnd, size_t index);
} // namespace playlist_utils
