#pragma once
#include "ui_widget.h"
#include <vector>
#include <string>
#include <functional>

class UIList : public UIWidget {
public:
    UIList();
    void set_items(const std::vector<std::string>& items);
    void set_selected(int index);
    int get_selected() const { return m_selectedIndex; }

    // Callback when selection changes (int = new index)
    void set_on_selection(std::function<void(int)> cb) { m_onSelection = cb; }

    void render(UIRenderer* ui) override;
    bool on_mouse_down(int x, int y, int button) override;
    bool on_mouse_up(int x, int y, int button) override;
    bool on_mouse_move(int x, int y) override;

private:
    std::vector<std::string> m_items;
    int m_selectedIndex = -1;
    int m_hoverIndex = -1;
    std::function<void(int)> m_onSelection;
};