#pragma once
#include "ui_widget.h"
#include <vector>
#include <string>
#include <functional>

class UIList : public UIWidget {
public:
    UIList();
    ~UIList() = default;

    void set_items(const std::vector<std::string>& labels);
    void set_items(const std::vector<std::pair<std::string, int>>& items);

    void set_selected(int index);
    int get_selected() const { return m_selected; }

    void set_scroll_offset(float offset) { m_scrollOffset = offset; }
    float get_scroll_offset() const { return m_scrollOffset; }
    float get_total_height() const { return m_items.size() * m_itemHeight; }
    float get_content_height() const override { return get_total_height(); }

    bool is_interactive() const override { return true; }

    void set_on_selection_changed(std::function<void(int)> callback) { m_onSelectionChanged = callback; }
    void set_on_reordered(std::function<void(const std::vector<int>&)> callback) { m_onReordered = callback; }

    void render(UIRenderer* ui) override;
    bool on_mouse_down(float x, float y, int button) override;
    bool on_mouse_up(float x, float y, int button) override;
    bool on_mouse_move(float x, float y) override;

private:
    struct Item {
        std::string label;
        int data;
    };
    std::vector<Item> m_items;
    int m_selected = -1;
    int m_hoveredIndex = -1;
    float m_itemHeight = 20.0f;
    float m_scrollOffset = 0.0f;

    std::function<void(int)> m_onSelectionChanged;
    std::function<void(const std::vector<int>&)> m_onReordered;

    bool m_dragging = false;
    int m_dragStartIndex = -1;
    int m_dragCurrentIndex = -1;
    float m_dragOffsetY = 0.0f;

    int get_item_at_y(float y) const;
    void reorder_items(int from, int to);
};