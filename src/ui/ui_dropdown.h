#pragma once
#include "ui_widget.h"
#include <vector>
#include <string>
#include <functional>

class UIDropdown : public UIWidget {
public:
    using OnSelect = std::function<void(int)>;

    void set_options(const std::vector<std::string>& opts) {
        m_options = opts;
        m_selected = -1;
    }
    void set_selected(int idx) { m_selected = idx; }
    int get_selected() const { return m_selected; }
    void set_on_select(OnSelect cb) { m_onSelect = cb; }

    void render(UIRenderer* renderer) override {
        // Background
        renderer->draw_rect(m_rect.x, m_rect.y, m_rect.w, m_rect.h,
            m_style.colors.textField.x, m_style.colors.textField.y,
            m_style.colors.textField.z, 1.0f);
        // Current selection text
        std::string text = (m_selected >=0 && m_selected < (int)m_options.size()) ?
                            m_options[m_selected] : "";
        renderer->draw_text(m_rect.x + 2, m_rect.y + 2, text.c_str(),
            m_style.colors.text.x, m_style.colors.text.y,
            m_style.colors.text.z, m_style.colors.text.w);
        // Arrow
        renderer->draw_text(m_rect.x + m_rect.w - 16, m_rect.y + 2, "\u25BC",
            m_style.colors.text.x, m_style.colors.text.y,
            m_style.colors.text.z, m_style.colors.text.w);

        // Dropdown list if expanded
        if (m_expanded) {
            float listY = m_rect.y + m_rect.h;
            float listH = m_options.size() * m_style.listItemHeight;
            renderer->draw_rect(m_rect.x, listY, m_rect.w, listH,
                m_style.colors.background.x, m_style.colors.background.y,
                m_style.colors.background.z, 1.0f);
            for (size_t i = 0; i < m_options.size(); ++i) {
                float y = listY + i * m_style.listItemHeight;
                bool hover = (i == m_hoverIndex);
                Vec4 bg = hover ? m_style.colors.listItemHover : m_style.colors.listItem;
                renderer->draw_rect(m_rect.x, y, m_rect.w, m_style.listItemHeight,
                    bg.x, bg.y, bg.z, bg.w);
                renderer->draw_text(m_rect.x + 2, y + 2, m_options[i].c_str(),
                    m_style.colors.text.x, m_style.colors.text.y,
                    m_style.colors.text.z, m_style.colors.text.w);
            }
        }
    }

    bool on_mouse_down(int x, int y) override {
        if (!hit_test(x, y)) {
            m_expanded = false;
            return false;
        }
        m_expanded = !m_expanded;
        return true;
    }

    bool on_mouse_move(int x, int y) override {
        m_hoverIndex = -1;
        if (m_expanded) {
            float listY = m_rect.y + m_rect.h;
            for (size_t i = 0; i < m_options.size(); ++i) {
                float y = listY + i * m_style.listItemHeight;
                if (x >= m_rect.x && x <= m_rect.x + m_rect.w &&
                    y >= y && y <= y + m_style.listItemHeight) {
                    m_hoverIndex = (int)i;
                    break;
                }
            }
        }
        return false;
    }

    bool on_mouse_up(int x, int y) override {
        if (m_expanded && m_hoverIndex >= 0) {
            m_selected = m_hoverIndex;
            if (m_onSelect) m_onSelect(m_selected);
            m_expanded = false;
            return true;
        }
        return false;
    }

private:
    std::vector<std::string> m_options;
    int m_selected = -1;
    int m_hoverIndex = -1;
    bool m_expanded = false;
    OnSelect m_onSelect;
};