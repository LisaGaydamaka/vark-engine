#pragma once
#include "ui_widget.h"
#include <functional>

class UISplitter : public UIWidget {
public:
    enum class Orientation { Horizontal, Vertical };

    UISplitter(Orientation orient = Orientation::Vertical);
    ~UISplitter() = default;

    void set_ratio(float ratio);
    float get_ratio() const { return m_ratio; }

    void set_handle_size(float size) { m_handleSize = size; }
    float get_handle_size() const { return m_handleSize; }

    void set_on_resize(std::function<void(float)> callback) { m_onResize = callback; }

    void layout() override;
    void render(UIRenderer* ui) override;

    bool on_mouse_down(float x, float y, int button) override;
    bool on_mouse_up(float x, float y, int button) override;
    bool on_mouse_move(float x, float y) override;

private:
    Orientation m_orientation;
    float m_ratio = 0.5f;
    float m_handleSize = 4.0f;
    bool m_dragging = false;
    std::function<void(float)> m_onResize;

    void update_child_rects();
};