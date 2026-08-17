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
    if (to < 0 || to > (int)m_items.size()) return;

    Item item = m_items[from];
    m_items.erase(m_items.begin() + from);
    if (to > from) --to;
    m_items.insert(m_items.begin() + to, item);

    if (m_selected == from) {
        m_selected = to;
    } else if (m_selected > from && m_selected <= to) {
        m_selected--;
    } else if (m_selected < from && m_selected >= to) {
        m_selected++;
    }
    if (m_onReordered) {
        std::vector<int> order;
        order.reserve(m_items.size());
        for (const auto& item : m_items) {
            order.push_back(item.data);
        }
        m_onReordered(order);
    }
}

// ---- render with viewport culling and performance logging ----
void UIList::render(UIRenderer* ui) {
    ui->push_clip_rect(m_rect.x, m_rect.y, m_rect.w, m_rect.h);

    float itemHeight = m_itemHeight;
    const float fontHeight = 8.0f;
    const float textOffsetY = (itemHeight - fontHeight) * 0.5f;

    // Compute how many items fit in the visible area
    int maxVisible = 0;
    if (itemHeight > 0.0f) {
        maxVisible = (int)(m_rect.h / itemHeight) + 1;   // +1 for partial last item
    }
    int endIndex = std::min((int)m_items.size(), maxVisible);

    // ---- PERFORMANCE LOG: show how many items we actually draw ----
    static int frameCounter = 0;
    ++frameCounter;
    // Log every 60 frames to avoid spam, but you can remove the condition for every frame
    if (frameCounter % 60 == 0) {
        LOG_INFO("UIList: rendering %d of %d items (visible height = %.1f, itemHeight = %.1f)",
                 endIndex, (int)m_items.size(), m_rect.h, itemHeight);
    }

    float y = m_rect.y;
    for (int i = 0; i < endIndex; ++i) {
        const Item& item = m_items[i];
        bool selected = (i == m_selected);
        bool hovered = (i == m_hoveredIndex && !m_dragging);

        // Background for selected/hover
        if (selected) {
            ui->draw_rect(m_rect.x, y, m_rect.w, itemHeight, 0.3f, 0.5f, 0.8f, 0.8f);
        } else if (hovered) {
            ui->draw_rect(m_rect.x, y, m_rect.w, itemHeight, 0.3f, 0.3f, 0.3f, 0.5f);
        }

        // Draw insertion line if dragging and this is the target
        if (m_dragging && i == m_dragCurrentIndex && i != m_dragStartIndex) {
            ui->draw_rect(m_rect.x + 4, y - 1, m_rect.w - 8, 2, 1.0f, 1.0f, 0.0f, 1.0f);
        }

        // Label
        float r = selected ? 1.0f : 0.9f;
        float g = selected ? 1.0f : 0.9f;
        float b = selected ? 0.8f : 0.9f;
        ui->draw_text(m_rect.x + 4.0f, y + textOffsetY, item.label.c_str(), r, g, b, 1.0f);

        y += itemHeight;
    }

    // Draw insertion line after the last item if dragging and the target is at the end
    if (m_dragging && m_dragCurrentIndex == (int)m_items.size()) {
        float yLine = m_rect.y + m_items.size() * m_itemHeight;
        // Only draw if the line is within the visible area
        if (yLine >= m_rect.y && yLine <= m_rect.y + m_rect.h) {
            ui->draw_rect(m_rect.x + 4, yLine - 1, m_rect.w - 8, 2, 1.0f, 1.0f, 0.0f, 1.0f);
        }
    }

    ui->pop_clip_rect();
}

// ---- Input handling (unchanged) ----
bool UIList::on_mouse_down(float x, float y, int button) {
    if (button != 0) return false;

    int index = get_item_at_y(y);
    if (index >= 0 && index < (int)m_items.size()) {
        if (m_items.size() > 1) {
            m_dragging = true;
            m_dragStartIndex = index;
            m_dragCurrentIndex = index;
            m_dragOffsetY = y - (m_rect.y + index * m_itemHeight);
            UIRoot* root = get_root();
            if (root) {
                root->set_capture(this, true);
            }
            m_selected = index;
            if (m_onSelectionChanged) {
                m_onSelectionChanged(index);
            }
            return true;
        } else {
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
        int from = m_dragStartIndex;
        int to = m_dragCurrentIndex;
        if (from >= 0 && to >= 0 && from != to) {
            reorder_items(from, to);
        }
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
        int index = get_item_at_y(y);
        if (index < 0) {
            if (y < m_rect.y) {
                index = 0;
            } else {
                index = (int)m_items.size();
            }
        } else {
            float relY = y - (m_rect.y + index * m_itemHeight);
            if (relY > m_itemHeight * 0.5f) {
                index = index + 1;
            }
        }
        if (index < 0) index = 0;
        if (index > (int)m_items.size()) index = (int)m_items.size();
        m_dragCurrentIndex = index;
        return true;
    } else {
        int index = get_item_at_y(y);
        if (index >= 0 && index < (int)m_items.size()) {
            m_hoveredIndex = index;
        } else {
            m_hoveredIndex = -1;
        }
        return false;
    }
}