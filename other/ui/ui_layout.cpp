#include "ui_layout.h"
#include <algorithm>
#include <numeric>

void UIVBoxLayout::apply(UIWidget* parent) {
    if (!parent) return;
    const auto& children = parent->get_children();
    if (children.empty()) return;

    // Get available area with padding
    float x = parent->get_rect().x + m_paddingLeft;
    float y = parent->get_rect().y + m_paddingTop;
    float availWidth = parent->get_rect().w - m_paddingLeft - m_paddingRight;
    float availHeight = parent->get_rect().h - m_paddingTop - m_paddingBottom;

    // Compute total preferred height
    float totalPreferred = 0.0f;
    int childCount = (int)children.size();
    for (auto& child : children) {
        totalPreferred += child->get_preferred_height();
    }
    // Add spacing between children
    totalPreferred += (childCount - 1) * m_spacing;

    // Compute extra height to distribute
    float extra = availHeight - totalPreferred;
    float extraPerChild = 0.0f;
    float totalStretch = 0.0f;
    for (auto& child : children) {
        totalStretch += child->get_stretch();
    }

    // We'll iterate and assign sizes
    float currentY = y;
    for (auto& child : children) {
        float childH = child->get_preferred_height();
        if (extra > 0.0f) {
            if (totalStretch > 0.0f) {
                // distribute extra proportionally to stretch
                float share = (child->get_stretch() / totalStretch) * extra;
                childH += share;
            } else {
                // no stretch – distribute equally
                childH += extra / childCount;
            }
        }
        // Ensure child height doesn't go negative
        if (childH < 0.0f) childH = 0.0f;

        child->set_rect(x, currentY, availWidth, childH);
        currentY += childH + m_spacing;
    }
}

void UIHBoxLayout::apply(UIWidget* parent) {
    if (!parent) return;
    const auto& children = parent->get_children();
    if (children.empty()) return;

    float x = parent->get_rect().x + m_paddingLeft;
    float y = parent->get_rect().y + m_paddingTop;
    float availWidth = parent->get_rect().w - m_paddingLeft - m_paddingRight;
    float availHeight = parent->get_rect().h - m_paddingTop - m_paddingBottom;

    float totalPreferred = 0.0f;
    int childCount = (int)children.size();
    for (auto& child : children) {
        totalPreferred += child->get_preferred_width();
    }
    totalPreferred += (childCount - 1) * m_spacing;

    float extra = availWidth - totalPreferred;
    float totalStretch = 0.0f;
    for (auto& child : children) {
        totalStretch += child->get_stretch();
    }

    float currentX = x;
    for (auto& child : children) {
        float childW = child->get_preferred_width();
        if (extra > 0.0f) {
            if (totalStretch > 0.0f) {
                float share = (child->get_stretch() / totalStretch) * extra;
                childW += share;
            } else {
                childW += extra / childCount;
            }
        }
        if (childW < 0.0f) childW = 0.0f;

        child->set_rect(currentX, y, childW, availHeight);
        currentX += childW + m_spacing;
    }
}

void UIFillLayout::apply(UIWidget* parent) {
    if (!parent) return;
    const auto& children = parent->get_children();
    if (children.empty()) return;

    float x = parent->get_rect().x + m_paddingLeft;
    float y = parent->get_rect().y + m_paddingTop;
    float w = parent->get_rect().w - m_paddingLeft - m_paddingRight;
    float h = parent->get_rect().h - m_paddingTop - m_paddingBottom;

    for (auto& child : children) {
        child->set_rect(x, y, w, h);
    }
}