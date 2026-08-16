#pragma once
#include "ui_widget.h"
#include <memory>

enum class SplitterOrientation {
    Horizontal, // left/right
    Vertical    // top/bottom
};

class UISplitter : public UIWidget {
public:
    UISplitter(SplitterOrientation orientation = SplitterOrientation::Horizontal);
    ~UISplitter() = default;

    void set_first(std::unique_ptr<UIWidget> widget);
    void set_second(std::unique_ptr<UIWidget> widget);
    void set_ratio(float ratio); // 0..1
    float get_ratio() const { return m_ratio; }
    void set_handle_size(float size) { m_handleSize = size; }

    void layout() override;
    void render(UIRenderer* ui) override;

    bool on_mouse_down(float x, float y, int button) override;
    bool on_mouse_up(float x, float y, int button) override;
    bool on_mouse_move(float x, float y) override;

private:
    SplitterOrientation m_orientation;
    float m_ratio = 0.5f;
    float m_handleSize = 4.0f;
    std::unique_ptr<UIWidget> m_first;
    std::unique_ptr<UIWidget> m_second;
    bool m_dragging = false;
    float m_dragStartPos = 0.0f;
    float m_dragStartRatio = 0.0f;

    bool is_over_handle(float x, float y) const;
    float get_handle_position() const;
};