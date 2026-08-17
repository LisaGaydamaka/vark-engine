#pragma once
#include "ui_widget.h"
#include <string>

class UILabel : public UIWidget {
public:
    UILabel(const std::string& text = "", float r=1, float g=1, float b=1, float a=1)
        : m_text(text), m_r(r), m_g(g), m_b(b), m_a(a) {}

    void set_text(const std::string& text) { m_text = text; }
    void set_color(float r, float g, float b, float a) { m_r = r; m_g = g; m_b = b; m_a = a; }

    void render(UIRenderer* ui) override {
        if (m_text.empty()) return;
        ui->draw_text(m_rect.x, m_rect.y, m_text.c_str(), m_r, m_g, m_b, m_a);
    }

private:
    std::string m_text;
    float m_r, m_g, m_b, m_a;
};