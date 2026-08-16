#include "ui_text_field.h"
#include "ui_style.h"
#include <cstdio>
#include <algorithm>

UITextField::UITextField(float value, std::function<void(float)> onCommit)
    : m_value(value), m_onCommit(onCommit) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.3f", value);
    m_text = buf;
}

void UITextField::set_value(float v) {
    m_value = v;
    char buf[32];
    snprintf(buf, sizeof(buf), "%.3f", v);
    m_text = buf;
}

float UITextField::get_value() const { return m_value; }

void UITextField::render(UIRenderer* ui) {
    begin_clip(ui);
    // Background
    ui->draw_rect(m_x, m_y, m_width, m_height, 0.1f, 0.1f, 0.15f, 1.0f);
    // Border if focused
    if (m_focused) {
        ui->draw_rect(m_x, m_y, m_width, 2.0f, 0.0f, 1.0f, 0.0f, 1.0f); // green top border
    }
    // Text
    uint32_t color = m_focused ? UIStyle::COLOR_TEXT_SELECTED : UIStyle::COLOR_TEXT;
    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float bl = (color & 0xFF) / 255.0f;
    float a = ((color >> 24) & 0xFF) / 255.0f;
    ui->draw_text(m_x + 4, m_y + 4, m_text.c_str(), r, g, bl, a);
    end_clip(ui);
}

bool UITextField::on_mouse_down(int x, int y, int button) {
    if (button == 0) {
        m_focused = true;
        return true;
    }
    return false;
}

bool UITextField::on_key_down(int key, bool ctrl, bool shift) {
    (void)ctrl; (void)shift;
    if (key == 13) { // Enter
        commit();
        return true;
    }
    if (key == 27) { // Escape
        // revert to original value
        set_value(m_value);
        m_focused = false;
        return true;
    }
    return false;
}

bool UITextField::on_text_input(char c) {
    if (!m_focused) return false;
    if (c == '\r' || c == '\n') {
        commit();
        return true;
    }
    if (c == 0x08) { // backspace
        if (!m_text.empty()) m_text.pop_back();
        return true;
    }
    if ((c >= '0' && c <= '9') || c == '-' || c == '.') {
        m_text.push_back(c);
        return true;
    }
    return false;
}

void UITextField::commit() {
    if (!m_text.empty()) {
        try {
            float val = std::stof(m_text);
            m_value = val;
            if (m_onCommit) m_onCommit(val);
        } catch (...) {}
    }
    m_focused = false;
}