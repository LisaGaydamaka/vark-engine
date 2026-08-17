#pragma once
#include "ui_widget.h"
#include "ui_style.h"

class UIPanel : public UIWidget {
public:
    UIPanel() {
        // Default colours from style
        m_bgR = UIStyle::panelBgR;
        m_bgG = UIStyle::panelBgG;
        m_bgB = UIStyle::panelBgB;
        m_bgA = UIStyle::panelBgA;
        m_borderR = UIStyle::panelBorderR;
        m_borderG = UIStyle::panelBorderG;
        m_borderB = UIStyle::panelBorderB;
        m_borderA = UIStyle::panelBorderA;
    }

    void set_background(float r, float g, float b, float a) {
        m_bgR = r; m_bgG = g; m_bgB = b; m_bgA = a;
    }

    void set_border(float r, float g, float b, float a, float thickness = 1.0f) {
        m_borderR = r; m_borderG = g; m_borderB = b; m_borderA = a;
        m_borderThickness = thickness;
    }

    void render(UIRenderer* ui) override {
        ui->draw_rect(m_rect.x, m_rect.y, m_rect.w, m_rect.h,
                      m_bgR, m_bgG, m_bgB, m_bgA);

        if (m_borderA > 0.0f) {
            float t = m_borderThickness;
            ui->draw_rect(m_rect.x, m_rect.y, m_rect.w, t,
                          m_borderR, m_borderG, m_borderB, m_borderA);
            ui->draw_rect(m_rect.x, m_rect.y + m_rect.h - t, m_rect.w, t,
                          m_borderR, m_borderG, m_borderB, m_borderA);
            ui->draw_rect(m_rect.x, m_rect.y, t, m_rect.h,
                          m_borderR, m_borderG, m_borderB, m_borderA);
            ui->draw_rect(m_rect.x + m_rect.w - t, m_rect.y, t, m_rect.h,
                          m_borderR, m_borderG, m_borderB, m_borderA);
        }
    }

private:
    float m_bgR, m_bgG, m_bgB, m_bgA;
    float m_borderR, m_borderG, m_borderB, m_borderA;
    float m_borderThickness = 1.0f;
};