#pragma once
#include "ui_widget.h"
#include "common/core/logger.h"
#include <functional>
#include <string>

class UIButton : public UIWidget {
public:
    UIButton(const std::string& label = "Button")
        : m_label(label) {}

    void set_label(const std::string& label) { m_label = label; }
    void set_on_click(std::function<void()> callback) { m_onClick = callback; }

    void render(UIRenderer* ui) override {
        float r = 0.2f, g = 0.3f, b = 0.5f, a = 1.0f;
        if (m_pressed) {
            r = 0.1f; g = 0.2f; b = 0.4f;
        } else if (m_hovered) {
            r = 0.3f; g = 0.4f; b = 0.6f;
        }
        ui->draw_rect(m_rect.x, m_rect.y, m_rect.w, m_rect.h, r, g, b, a);

        ui->draw_rect(m_rect.x, m_rect.y, m_rect.w, 1.0f, 0.7f, 0.7f, 0.7f, 1.0f);
        ui->draw_rect(m_rect.x, m_rect.y + m_rect.h - 1.0f, m_rect.w, 1.0f, 0.3f, 0.3f, 0.3f, 1.0f);
        ui->draw_rect(m_rect.x, m_rect.y, 1.0f, m_rect.h, 0.7f, 0.7f, 0.7f, 1.0f);
        ui->draw_rect(m_rect.x + m_rect.w - 1.0f, m_rect.y, 1.0f, m_rect.h, 0.3f, 0.3f, 0.3f, 1.0f);

        float labelX = m_rect.x + 8.0f;
        float labelY = m_rect.y + (m_rect.h - 8.0f) * 0.5f;
        ui->draw_text(labelX, labelY, m_label.c_str(), 1.0f, 1.0f, 1.0f, 1.0f);
    }

    bool on_mouse_down(float x, float y, int button) override {
        LOG_INFO("UIButton: on_mouse_down(%f, %f, %d)", x, y, button);
        if (button == 0) {
            m_pressed = true;
            LOG_INFO("UIButton: Pressed = true");
            return true;
        }
        return false;
    }

    bool on_mouse_up(float x, float y, int button) override {
        LOG_INFO("UIButton: on_mouse_up(%f, %f, %d)", x, y, button);
        if (button == 0 && m_pressed) {
            m_pressed = false;
            LOG_INFO("UIButton: Clicked! Calling callback.");
            if (m_onClick) {
                m_onClick();
            }
            return true;
        }
        return false;
    }

    bool on_mouse_move(float x, float y) override {
        bool inside = m_rect.contains(x, y);
        if (inside != m_hovered) {
            m_hovered = inside;
        }
        return false;
    }

private:
    std::string m_label;
    std::function<void()> m_onClick;
    bool m_hovered = false;
    bool m_pressed = false;
};