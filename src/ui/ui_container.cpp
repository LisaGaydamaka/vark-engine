#include "ui_container.h"
#include <algorithm>

// ----- UILayout -----
UILayout::UILayout(Orientation orientation, float spacing)
    : m_orientation(orientation), m_spacing(spacing) {}

void UILayout::layout() {
    if (m_children.empty()) return;

    float totalSpacing = m_spacing * (m_children.size() - 1);
    float totalSize = (m_orientation == Orientation::Horizontal) ? m_width : m_height;
    float available = totalSize - totalSpacing;
    if (available < 0) available = 0;

    // Determine stretch size
    float stretchSize = 0;
    int stretchCount = 0;
    if (m_stretchIdx >= 0 && m_stretchIdx < (int)m_children.size()) {
        stretchCount = 1;
    } else {
        // no stretch, all children get equal share
        stretchCount = (int)m_children.size();
    }

    float fixedSizes = 0;
    for (size_t i = 0; i < m_children.size(); ++i) {
        if (m_stretchIdx >= 0 && (int)i == m_stretchIdx) continue;
        // fixed size: we need a way to get preferred size; for now assume equal distribution
        // We'll just allocate equally among non-stretch children.
        fixedSizes += available / (m_children.size() - (m_stretchIdx >= 0 ? 1 : 0));
    }

    float currentPos = 0;
    for (size_t i = 0; i < m_children.size(); ++i) {
        UIWidget* child = m_children[i].get();
        float size;
        if (m_stretchIdx >= 0 && (int)i == m_stretchIdx) {
            size = available - fixedSizes; // stretch takes remaining
        } else {
            size = available / (m_children.size() - (m_stretchIdx >= 0 ? 1 : 0));
        }
        if (m_orientation == Orientation::Horizontal) {
            child->set_rect(m_x + currentPos, m_y, size, m_height);
        } else {
            child->set_rect(m_x, m_y + currentPos, m_width, size);
        }
        currentPos += size + m_spacing;
    }
}

void UILayout::render(UIRenderer* ui) {
    for (auto& child : m_children) {
        child->render(ui);
    }
}

// ----- UIFillLayout -----
void UIFillLayout::layout() {
    for (auto& child : m_children) {
        child->set_rect(m_x, m_y, m_width, m_height);
    }
}

void UIFillLayout::render(UIRenderer* ui) {
    for (auto& child : m_children) {
        child->render(ui);
    }
}