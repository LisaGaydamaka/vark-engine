#include "ui_root.h"
#include "core/logger.h"
#include <cstdio>

static UIWidget* find_deepest_widget(UIWidget* parent, float x, float y) {
    const auto& children = parent->get_children();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        UIWidget* child = it->get();
        if (child->hit_test(x, y)) {
            UIWidget* deeper = find_deepest_widget(child, x, y);
            if (deeper) return deeper;
            return child;
        }
    }
    return nullptr;
}

UIWidget* UIRoot::find_widget_at(float x, float y) {
    return find_deepest_widget(this, x, y);
}

void UIRoot::render_all(UIRenderer* ui) {
    layout_all();
    render(ui);
    for (auto& child : m_children) {
        child->render_all(ui);
    }
}

bool UIRoot::on_mouse_down(float x, float y, int button) {
    // Release any previous capture
    release_mouse_capture();

    UIWidget* target = find_widget_at(x, y);
    if (target) {
        if (target->on_mouse_down(x, y, button)) {
            // Widget consumed the event; capture it
            set_mouse_capture(target);
            return true;
        }
    }
    return false;
}

bool UIRoot::on_mouse_up(float x, float y, int button) {
    if (m_captureWidget) {
        bool consumed = m_captureWidget->on_mouse_up(x, y, button);
        release_mouse_capture();
        return consumed;
    }
    UIWidget* target = find_widget_at(x, y);
    if (target) {
        return target->on_mouse_up(x, y, button);
    }
    return false;
}

bool UIRoot::on_mouse_move(float x, float y) {
    if (m_captureWidget) {
        return m_captureWidget->on_mouse_move(x, y);
    }
    UIWidget* target = find_widget_at(x, y);
    if (target) {
        return target->on_mouse_move(x, y);
    }
    return false;
}

bool UIRoot::on_key_down(int key, bool ctrl, bool shift) {
    // For now, return false
    return false;
}

bool UIRoot::on_char(char c) {
    return false;
}