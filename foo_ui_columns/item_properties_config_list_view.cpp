#include "stdafx.h"
#include "item_properties.h"

namespace cui::panels::item_properties {

void FieldsList::notify_save_inline_edit(const char* value)
{
    if (m_edit_index < m_fields.get_count()) {
        (m_edit_column ? m_fields[m_edit_index].m_name : m_fields[m_edit_index].m_name_friendly) = value;
        pfc::list_t<SizedInsertItem<2, 0>> items;
        items.set_count(1);
        items[0].m_subitems[0] = m_fields[m_edit_index].m_name_friendly;
        items[0].m_subitems[1] = m_fields[m_edit_index].m_name;
        replace_items(m_edit_index, items);
    }
    m_edit_column = pfc_infinite;
    m_edit_index = pfc_infinite;
}

bool FieldsList::notify_create_inline_edit(const pfc::list_base_const_t<t_size>& indices, unsigned column,
    pfc::string_base& p_text, t_size& p_flags, mmh::ComPtr<IUnknown>& pAutocompleteEntries)
{
    t_size indices_count = indices.get_count();
    if (indices_count == 1 && indices[0] < m_fields.get_count()) {
        m_edit_index = indices[0];
        m_edit_column = column;

        p_text = m_edit_column ? m_fields[m_edit_index].m_name : m_fields[m_edit_index].m_name_friendly;

        if (m_edit_column == 1)
            p_flags = inline_edit_uppercase;

        return true;
    }
    return false;
}

bool FieldsList::notify_before_create_inline_edit(
    const pfc::list_base_const_t<t_size>& indices, unsigned column, bool b_source_mouse)
{
    return column <= 1 && indices.get_count() == 1;
}

void FieldsList::notify_on_create()
{
    set_single_selection(true);
    set_columns({{"Name", 150}, {"Field", 150}});

    t_size count = m_fields.get_count();
    pfc::list_t<InsertItem> items;
    get_insert_items(0, count, items);
    insert_items(0, count, items.get_ptr());
}

void FieldsList::get_insert_items(t_size base, t_size count, pfc::list_t<InsertItem>& items)
{
    items.set_count(count);
    for (t_size i = 0; i < count; i++) {
        items[i].m_subitems.resize(2);
        items[i].m_subitems[0] = m_fields[base + i].m_name_friendly;
        items[i].m_subitems[1] = m_fields[base + i].m_name;
    }
}

FieldsList::FieldsList(pfc::list_t<Field>& p_fields)
    : m_edit_index(pfc_infinite)
    , m_edit_column(pfc_infinite)
    , m_fields(p_fields)
{
}

} // namespace cui::panels::item_properties
