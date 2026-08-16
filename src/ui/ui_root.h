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

    void set_mouse_capture(UIWidget* widget) { m_captureWidget = widget; }
    void release_mouse_capture() { m_captureWidget = nullptr; }

private:
    UIWidget* find_widget_at(float x, float y);
    UIWidget* m_captureWidget = nullptr;
};