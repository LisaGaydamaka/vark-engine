#include "ui_button.h"
#include "ui_style.h"

UIButton::UIButton(const std::string& label, std::function<void()> onClick)
    : m_label(label), m_onClick(onClick) {}

void UIButton::render(UIRenderer* ui) {
    begin_clip(ui);
    uint32_t color = m_pressed ? UIStyle::COLOR_BUTTON_HOVER : m_bgColor;
    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float bl = (color & 0xFF) / 255.0f;
    float a = ((color >> 24) & 0xFF) / 255.0f;
    ui->draw_rect(m_x, m_y, m_width, m_height, r, g, bl, a);

    // Text centered
    float tw = m_label.length() * UIStyle::FONT_CHAR_WIDTH;
    float tx = m_x + (m_width - tw) / 2;
    float ty = m_y + (m_height - UIStyle::FONT_CHAR_HEIGHT) / 2;
    ui->draw_text(tx, ty, m_label.c_str(), 1, 1, 1, 1);
    end_clip(ui);
}

bool UIButton::on_mouse_down(int x, int y, int button) {
    if (button == 0) {
        m_pressed = true;
        return true;
    }
    return false;
}

bool UIButton::on_mouse_up(int x, int y, int button) {
    if (button == 0 && m_pressed) {
        m_pressed = false;
        if (m_onClick) m_onClick();
        return true;
    }
    return false;
}