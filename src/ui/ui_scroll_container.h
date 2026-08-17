#pragma once
#include "ui_widget.h"
#include "ui_scrollbar.h"
#include <memory>

class UIScrollContainer : public UIWidget {
public:
    UIScrollContainer();
    ~UIScrollContainer() = default;

    void set_child(std::unique_ptr<UIWidget> child);
    void set_scrollbar_width(float width) { m_scrollbarWidth = width; }
    UIWidget* get_child() const { return m_child; }

    void layout() override;
    void render(UIRenderer* ui) override;
    bool on_mouse_wheel(float delta, float x, float y) override;
    bool on_mouse_down(float x, float y, int button) override;
    bool on_mouse_up(float x, float y, int button) override;
    bool on_mouse_move(float x, float y) override;

private:
    UIWidget* m_child = nullptr;
    UIScrollBar* m_scrollbar = nullptr;
    float m_scrollbarWidth = 16.0f;
    float m_scrollOffset = 0.0f;
    float m_contentHeight = 0.0f;
    float m_viewportHeight = 0.0f;

    void update_scrollbar();
    void on_scrollbar_value_changed(float value);
};