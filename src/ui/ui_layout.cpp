#include "ui_layout.h"
#include <algorithm>

void UIVBoxLayout::apply(UIWidget* parent) {
    if (!parent) return;
    const auto& children = parent->get_children();
    if (children.empty()) return;

    float y = parent->get_rect().y;
    float x = parent->get_rect().x;
    float width = parent->get_rect().w;

    for (auto& child : children) {
        float childH = child->get_rect().h;
        child->set_rect(x, y, width, childH);
        y += childH + m_spacing;
    }
}

void UIHBoxLayout::apply(UIWidget* parent) {
    if (!parent) return;
    const auto& children = parent->get_children();
    if (children.empty()) return;

    float x = parent->get_rect().x;
    float y = parent->get_rect().y;
    float height = parent->get_rect().h;

    for (auto& child : children) {
        float childW = child->get_rect().w;
        child->set_rect(x, y, childW, height);
        x += childW + m_spacing;
    }
}

void UIFillLayout::apply(UIWidget* parent) {
    if (!parent) return;
    const auto& children = parent->get_children();
    if (children.empty()) return;

    float x = parent->get_rect().x;
    float y = parent->get_rect().y;
    float w = parent->get_rect().w;
    float h = parent->get_rect().h;

    for (auto& child : children) {
        child->set_rect(x, y, w, h);
    }
}