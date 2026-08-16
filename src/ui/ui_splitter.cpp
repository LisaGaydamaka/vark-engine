#include "ui_splitter.h"
#include <algorithm>

UISplitter::UISplitter(SplitterOrientation orientation)
    : m_orientation(orientation) {}

void UISplitter::set_first(std::unique_ptr<UIWidget> widget) {
    m_first = std::move(widget);
    if (m_first) m_first->m_parent = this;
}

void UISplitter::set_second(std::unique_ptr<UIWidget> widget) {
    m_second = std::move(widget);
    if (m_second) m_second->m_parent = this;
}

void UISplitter::set_ratio(float ratio) {
    m_ratio = std::clamp(ratio, 0.05f, 0.95f);
}

float UISplitter::get_handle_position() const {
    if (m_orientation == SplitterOrientation::Horizontal) {
        return m_rect.x + m_rect.w * m_ratio;
    } else {
        return m_rect.y + m_rect.h * m_ratio;
    }
}

bool UISplitter::is_over_handle(float x, float y) const {
    float pos = get_handle_position();
    float half = m_handleSize * 0.5f;
    if (m_orientation == SplitterOrientation::Horizontal) {
        return x >= pos - half && x <= pos + half && y >= m_rect.y && y <= m_rect.y + m_rect.h;
    } else {
        return y >= pos - half && y <= pos + half && x >= m_rect.x && x <= m_rect.x + m_rect.w;
    }
}

void UISplitter::layout() {
    if (!m_first || !m_second) return;

    float x = m_rect.x;
    float y = m_rect.y;
    float w = m_rect.w;
    float h = m_rect.h;

    if (m_orientation == SplitterOrientation::Horizontal) {
        float splitX = x + w * m_ratio;
        float handleHalf = m_handleSize * 0.5f;
        // First widget occupies from x to splitX - handleHalf
        m_first->set_rect(x, y, splitX - x - handleHalf, h);
        // Second widget occupies from splitX + handleHalf to x+w
        m_second->set_rect(splitX + handleHalf, y, x + w - splitX - handleHalf, h);
    } else { // Vertical
        float splitY = y + h * m_ratio;
        float handleHalf = m_handleSize * 0.5f;
        m_first->set_rect(x, y, w, splitY - y - handleHalf);
        m_second->set_rect(x, splitY + handleHalf, w, y + h - splitY - handleHalf);
    }

    // Layout children (if they have their own layouts)
    m_first->layout_all();
    m_second->layout_all();
}

void UISplitter::render(UIRenderer* ui) {
    // Draw handle
    float pos = get_handle_position();
    float half = m_handleSize * 0.5f;
    if (m_orientation == SplitterOrientation::Horizontal) {
        ui->draw_rect(pos - half, m_rect.y, m_handleSize, m_rect.h, 0.4f, 0.4f, 0.4f, 1.0f);
        // Draw grip dots (optional)
        for (float y = m_rect.y + 10; y < m_rect.y + m_rect.h - 10; y += 8) {
            ui->draw_rect(pos - 1, y, 2, 2, 0.7f, 0.7f, 0.7f, 1.0f);
        }
    } else {
        ui->draw_rect(m_rect.x, pos - half, m_rect.w, m_handleSize, 0.4f, 0.4f, 0.4f, 1.0f);
        for (float x = m_rect.x + 10; x < m_rect.x + m_rect.w - 10; x += 8) {
            ui->draw_rect(x, pos - 1, 2, 2, 0.7f, 0.7f, 0.7f, 1.0f);
        }
    }

    // Render children
    if (m_first) m_first->render_all(ui);
    if (m_second) m_second->render_all(ui);
}

bool UISplitter::on_mouse_down(float x, float y, int button) {
    if (button == 0 && is_over_handle(x, y)) {
        m_dragging = true;
        m_dragStartPos = (m_orientation == SplitterOrientation::Horizontal) ? x : y;
        m_dragStartRatio = m_ratio;
        return true;
    }
    // Pass event to children? Not necessary for splitter.
    return false;
}

bool UISplitter::on_mouse_up(float x, float y, int button) {
    if (button == 0 && m_dragging) {
        m_dragging = false;
        return true;
    }
    return false;
}

bool UISplitter::on_mouse_move(float x, float y) {
    if (m_dragging) {
        float pos = (m_orientation == SplitterOrientation::Horizontal) ? x : y;
        float delta = pos - m_dragStartPos;
        float total = (m_orientation == SplitterOrientation::Horizontal) ? m_rect.w : m_rect.h;
        if (total > 0) {
            float newRatio = m_dragStartRatio + delta / total;
            set_ratio(newRatio);
            // Trigger re-layout (will happen next frame)
        }
        return true;
    }
    return false;
}