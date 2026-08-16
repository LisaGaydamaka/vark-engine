#include "ui_panel.h"
#include "ui_style.h"

UIPanel::UIPanel(uint32_t bgColor) : m_bgColor(bgColor) {}

void UIPanel::render(UIRenderer* ui) {
    begin_clip(ui);
    // Draw background
    float r = ((m_bgColor >> 16) & 0xFF) / 255.0f;
    float g = ((m_bgColor >> 8) & 0xFF) / 255.0f;
    float b = (m_bgColor & 0xFF) / 255.0f;
    float a = ((m_bgColor >> 24) & 0xFF) / 255.0f;
    ui->draw_rect(m_x, m_y, m_width, m_height, r, g, b, a);

    // Draw children
    for (auto& child : m_children) {
        child->render(ui);
    }
    end_clip(ui);
}