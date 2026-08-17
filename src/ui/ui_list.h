#pragma once
#include "ui_widget.h"
#include <vector>
#include <string>
#include <functional>

class UIList : public UIWidget {
public:
    UIList();
    ~UIList() = default;

    // Set items (each with a label and an associated data value)
    void set_items(const std::vector<std::string>& labels);
    void set_items(const std::vector<std::pair<std::string, int>>& items);

    void set_selected(int index);          // index in the items list
    int get_selected() const { return m_selected; }

    // Callback when selection changes: parameter is the new selected index, or -1 if none
    void set_on_selection_changed(std::function<void(int)> callback) { m_onSelectionChanged = callback; }
    // Callback when items are reordered: parameter is the new order (list of indices)
    void set_on_reordered(std::function<void(const std::vector<int>&)> callback) { m_onReordered = callback; }

    void render(UIRenderer* ui) override;
    bool on_mouse_down(float x, float y, int button) override;
    bool on_mouse_up(float x, float y, int button) override;
    bool on_mouse_move(float x, float y) override;

private:
    struct Item {
        std::string label;
        int data;   // user‑defined value (e.g., brush index)
    };
    std::vector<Item> m_items;
    int m_selected = -1;
    int m_hoveredIndex = -1;
    float m_itemHeight = 20.0f;
    std::function<void(int)> m_onSelectionChanged;
    std::function<void(const std::vector<int>&)> m_onReordered;

    // Drag state
    bool m_dragging = false;
    int m_dragStartIndex = -1;
    int m_dragCurrentIndex = -1;  // where the item would be inserted
    float m_dragOffsetY = 0.0f;   // offset from top of item to mouse

    // Helper: get the index of the item at the given Y coordinate (relative to list rect)
    int get_item_at_y(float y) const;
    // Helper: reorder items: move item from 'from' to 'to' (insert before 'to')
    void reorder_items(int from, int to);
};