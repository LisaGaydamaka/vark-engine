#include "ui_list.h"
#include "ui_renderer.h"
#include "ui_root.h"
#include "core/logger.h"
#include <algorithm>

UIList::UIList() = default;

void UIList::set_items(const std::vector<std::string>& labels) {
    m_items.clear();
    m_items.reserve(labels.size());
    for (size_t i = 0; i < labels.size(); ++i) {
        m_items.push_back({labels[i], static_cast<int>(i)});
    }
    m_selected = -1;
    m_dragging = false;
}

void UIList::set_items(const std::vector<std::pair<std::string, int>>& items) {
    m_items.clear();
    m_items.reserve(items.size());
    for (const auto& p : items) {
        m_items.push_back({p.first, p.second});
    }
    m_selected = -1;
    m_dragging = false;
}

void UIList::set_selected(int index) {
    if (index >= 0 && index < (int)m_items.size()) {
        m_selected = index;
    } else {
        m_selected = -1;
    }
}

int UIList::get_item_at_y(float y) const {
    float relY = y - m_rect.y;
    int index = (int)(relY / m_itemHeight);
    if (index < 0) return -1;
    if (index >= (int)m_items.size()) return -1;
    return index;
}

void UIList::reorder_items(int from, int to) {
    if (from == to) return;
    if (from < 0 || from >= (int)m_items.size()) return;
    if (to < 0 || to > (int)m_items.size()) return; // to can be size (insert at end)

    Item item = m_items[from];
    m_items.erase(m_items.begin() + from);
    // Insert before 'to' (if to > from, we need to adjust because erase shifts)
    if (to > from) --to;
    m_items.insert(m_items.begin() + to, item);

    // Update selection if needed
    if (m_selected == from) {
        m_selected = to;
    } else if (m_selected > from && m_selected <= to) {
        m_selected--;
    } else if (m_selected < from && m_selected >= to) {
        m_selected++;
    }
    // Notify reordered
    if (m_onReordered) {
        std::vector<int> order;
        order.reserve(m_items.size());
        for (const auto& item : m_items) {
            order.push_back(item.data);
        }
        m_onReordered(order);
    }
}

void UIList::render(UIRenderer* ui) {
    ui->push_clip_rect(m_rect.x, m_rect.y, m_rect.w, m_rect.h);

    float y = m_rect.y;
    float itemHeight = m_itemHeight;

    // Font height is 8px, we'll use that for vertical centering
    const float fontHeight = 8.0f;
    const float textOffsetY = (itemHeight - fontHeight) * 0.5f;

    for (int i = 0; i < (int)m_items.size(); ++i) {
        if (y + itemHeight > m_rect.y + m_rect.h) break;

        const Item& item = m_items[i];
        bool selected = (i == m_selected);
        bool hovered = (i == m_hoveredIndex && !m_dragging);

        // Background for selected/hover
        if (selected) {
            ui->draw_rect(m_rect.x, y, m_rect.w, itemHeight, 0.3f, 0.5f, 0.8f, 0.8f);
        } else if (hovered) {
            ui->draw_rect(m_rect.x, y, m_rect.w, itemHeight, 0.3f, 0.3f, 0.3f, 0.5f);
        }

        // If dragging and this is the drag target insertion point, draw a line
        if (m_dragging && i == m_dragCurrentIndex && i != m_dragStartIndex) {
            ui->draw_rect(m_rect.x + 4, y - 1, m_rect.w - 8, 2, 1.0f, 1.0f, 0.0f, 1.0f);
        }

        // Draw label (vertically centered)
        float r = selected ? 1.0f : 0.9f;
        float g = selected ? 1.0f : 0.9f;
        float b = selected ? 0.8f : 0.9f;
        ui->draw_text(m_rect.x + 4.0f, y + textOffsetY, item.label.c_str(), r, g, b, 1.0f);

        y += itemHeight;
    }

    // If dragging and the insertion point is after the last item, draw a line at the bottom
    if (m_dragging && m_dragCurrentIndex == (int)m_items.size()) {
        float yLine = m_rect.y + m_items.size() * m_itemHeight;
        ui->draw_rect(m_rect.x + 4, yLine - 1, m_rect.w - 8, 2, 1.0f, 1.0f, 0.0f, 1.0f);
    }

    ui->pop_clip_rect();
}

bool UIList::on_mouse_down(float x, float y, int button) {
    if (button != 0) return false;

    int index = get_item_at_y(y);
    if (index >= 0 && index < (int)m_items.size()) {
        // Start dragging if we have more than one item
        if (m_items.size() > 1) {
            m_dragging = true;
            m_dragStartIndex = index;
            m_dragCurrentIndex = index;
            m_dragOffsetY = y - (m_rect.y + index * m_itemHeight);
            // Capture mouse
            UIRoot* root = get_root();
            if (root) {
                root->set_capture(this, true);
            }
            // Keep selection
            m_selected = index;
            if (m_onSelectionChanged) {
                m_onSelectionChanged(index);
            }
            return true;
        } else {
            // Just select
            m_selected = index;
            if (m_onSelectionChanged) {
                m_onSelectionChanged(index);
            }
            return true;
        }
    }
    return false;
}

bool UIList::on_mouse_up(float x, float y, int button) {
    if (button != 0) return false;
    if (m_dragging) {
        // Drop: reorder items
        int from = m_dragStartIndex;
        int to = m_dragCurrentIndex;
        if (from >= 0 && to >= 0 && from != to) {
            reorder_items(from, to);
        }
        // End dragging
        m_dragging = false;
        m_dragStartIndex = -1;
        m_dragCurrentIndex = -1;
        UIRoot* root = get_root();
        if (root) {
            root->set_capture(this, false);
        }
        return true;
    }
    return false;
}

bool UIList::on_mouse_move(float x, float y) {
    if (m_dragging) {
        // Update hover/insertion point
        int index = get_item_at_y(y);
        if (index < 0) {
            // If mouse is above the list, insert at 0; if below, insert at end
            if (y < m_rect.y) {
                index = 0;
            } else {
                index = (int)m_items.size();
            }
        } else {
            // If mouse is below the item's middle, we want to insert after that item
            float relY = y - (m_rect.y + index * m_itemHeight);
            if (relY > m_itemHeight * 0.5f) {
                index = index + 1;
            }
        }
        // Clamp
        if (index < 0) index = 0;
        if (index > (int)m_items.size()) index = (int)m_items.size();
        m_dragCurrentIndex = index;
        return true;
    } else {
        // Update hovered index
        int index = get_item_at_y(y);
        if (index >= 0 && index < (int)m_items.size()) {
            m_hoveredIndex = index;
        } else {
            m_hoveredIndex = -1;
        }
        return false;
    }
}