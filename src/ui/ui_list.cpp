#include "ui_list.h"
#include "ui_style.h"

UIList::UIList() {}

void UIList::set_items(const std::vector<std::string>& items) {
    m_items = items;
    if (m_selectedIndex >= (int)m_items.size()) m_selectedIndex = -1;
}

void UIList::set_selected(int index) {
    if (index >= 0 && index < (int)m_items.size()) {
        m_selectedIndex = index;
        if (m_onSelection) m_onSelection(index);
    } else {
        m_selectedIndex = -1;
    }
}

void UIList::render(UIRenderer* ui) {
    begin_clip(ui);
    float y = m_y;
    float lineH = UIStyle::LINE_HEIGHT;
    for (size_t i = 0; i < m_items.size(); ++i) {
        bool selected = ((int)i == m_selectedIndex);
        bool hover = ((int)i == m_hoverIndex);
        // Background
        if (selected) {
            ui->draw_rect(m_x, y, m_width, lineH,
                          ((UIStyle::COLOR_BG_SELECTED >> 16)&0xFF)/255.0f,
                          ((UIStyle::COLOR_BG_SELECTED >> 8)&0xFF)/255.0f,
                          (UIStyle::COLOR_BG_SELECTED & 0xFF)/255.0f, 0.8f);
        } else if (hover) {
            ui->draw_rect(m_x, y, m_width, lineH, 0.2f, 0.2f, 0.3f, 0.5f);
        }
        uint32_t color = selected ? UIStyle::COLOR_TEXT_SELECTED : UIStyle::COLOR_TEXT;
        float r = ((color >> 16) & 0xFF) / 255.0f;
        float g = ((color >> 8) & 0xFF) / 255.0f;
        float bl = (color & 0xFF) / 255.0f;
        ui->draw_text(m_x + 4, y + 2, m_items[i].c_str(), r, g, bl, 1.0f);
        y += lineH;
        if (y > m_y + m_height) break;
    }
    end_clip(ui);
}

bool UIList::on_mouse_down(int x, int y, int button) {
    if (button != 0) return false;
    float lineH = UIStyle::LINE_HEIGHT;
    int index = (int)((y - m_y) / lineH);
    if (index >= 0 && index < (int)m_items.size()) {
        set_selected(index);
        return true;
    }
    return false;
}

bool UIList::on_mouse_up(int x, int y, int button) { return false; }

bool UIList::on_mouse_move(int x, int y) {
    float lineH = UIStyle::LINE_HEIGHT;
    int index = (int)((y - m_y) / lineH);
    if (index >= 0 && index < (int)m_items.size()) {
        m_hoverIndex = index;
    } else {
        m_hoverIndex = -1;
    }
    return true;
}