#pragma once
#include "ui_widget.h"
#include <functional>
#include <algorithm>

class UIScrollBar : public UIWidget {
public:
    enum Orientation { Vertical, Horizontal };

    UIScrollBar(Orientation orient = Vertical);

    void set_range(float min, float max);
    void set_value(float val);
    float get_value() const { return m_value; }

    void set_on_value_changed(std::function<void(float)> callback) { m_onValueChanged = callback; }

    // ---- NEW: interactive ----
    bool is_interactive() const override { return true; }

    void render(UIRenderer* ui) override;
    bool on_mouse_down(float x, float y, int button) override;
    bool on_mouse_up(float x, float y, int button) override;
    bool on_mouse_move(float x, float y) override;

private:
    Orientation m_orientation;
    float m_min = 0.0f;
    float m_max = 1.0f;
    float m_value = 0.0f;
    float m_thumbSize = 20.0f;
    bool m_dragging = false;
    float m_dragOffset = 0.0f;
    std::function<void(float)> m_onValueChanged;

    float get_track_length() const;
    float get_thumb_position() const;
    void update_thumb_size();
};