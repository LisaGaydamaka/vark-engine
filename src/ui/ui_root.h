#pragma once
#include "ui_widget.h"
#include <memory>

class UIRoot : public UIWidget {
public:
    UIRoot() = default;
    ~UIRoot() = default;

    bool hit_test(float x, float y) const override { return true; }

    void render_all(UIRenderer* ui) override;

    bool on_mouse_down(float x, float y, int button) override;
    bool on_mouse_up(float x, float y, int button) override;
    bool on_mouse_move(float x, float y) override;
    bool on_key_down(int key, bool ctrl, bool shift) override;
    bool on_char(char c) override;

    void set_capture(UIWidget* widget, bool capture);
    UIWidget* get_captured_widget() const { return m_capturedWidget; }

private:
    UIWidget* find_widget_at(float x, float y);
    UIWidget* m_capturedWidget = nullptr;
    UIWidget* m_hoveredWidget = nullptr;
};