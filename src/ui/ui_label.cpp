#include "ui_label.h"
#include "ui_style.h"

UILabel::UILabel(const std::string& text, uint32_t color) : m_text(text), m_color(color) {}

void UILabel::render(UIRenderer* ui) {
    begin_clip(ui);
    float r = ((m_color >> 16) & 0xFF) / 255.0f;
    float g = ((m_color >> 8) & 0xFF) / 255.0f;
    float b = (m_color & 0xFF) / 255.0f;
    float a = ((m_color >> 24) & 0xFF) / 255.0f;
    ui->draw_text(m_x, m_y, m_text.c_str(), r, g, b, a);
    end_clip(ui);
}