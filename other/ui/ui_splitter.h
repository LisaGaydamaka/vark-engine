#pragma once
#include "ui_widget.h"
#include <memory>

class UISplitter : public UIWidget {
public:
    enum class Orientation { Horizontal, Vertical };

    UISplitter(Orientation orient = Orientation::Vertical, float initialRatio = 0.5f);
    ~UISplitter() = default;

    void set_orientation(Orientation orient) { m_orientation = orient; }
    void set_ratio(float ratio) { m_ratio = ratio; }
    float get_ratio() const { return m_ratio; }

    void set_handle_thickness(float thickness) { m_handleThickness = thickness; }
    float get_handle_thickness() const { return m_handleThickness; }

    void set_hit_thickness(float thickness) { m_hitThickness = thickness; }
    float get_hit_thickness() const { return m_hitThickness; }

    void layout() override;
    void render(UIRenderer* ui) override;

    bool is_priority_hit(float x, float y) const override {
        return is_on_handle(x, y);
    }

    bool on_mouse_down(float x, float y, int button) override;
    bool on_mouse_up(float x, float y, int button) override;
    bool on_mouse_move(float x, float y) override;
    bool is_interactive() const override { return true; }

private:
    bool is_on_handle(float x, float y) const;
    void update_ratio_from_mouse(float x, float y);

    Orientation m_orientation = Orientation::Vertical;
    float m_ratio = 0.5f;
    float m_handleThickness = 4.0f;
    float m_hitThickness = 20.0f;
    bool m_dragging = false;
};