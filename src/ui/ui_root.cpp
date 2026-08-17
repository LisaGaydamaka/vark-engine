#include "ui_root.h"
#include "../common/core/logger.h"
#include <cstdio>
#include <typeinfo>

static UIWidget* find_deepest_widget(UIWidget* parent, float x, float y) {
    // ---- NEW: if the parent claims priority hit, return it immediately ----
    if (parent->is_priority_hit(x, y)) {
        return parent;
    }

    const auto& children = parent->get_children();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        UIWidget* child = it->get();
        if (child->hit_test(x, y)) {
            UIWidget* deeper = find_deepest_widget(child, x, y);
            if (deeper) {
                return deeper;
            }
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
    if (m_capturedWidget) {
        m_capturedWidget->on_mouse_down(x, y, button);
        return true;
    }
    UIWidget* target = find_widget_at(x, y);
    if (target) {
        return target->on_mouse_down(x, y, button);
    }
    return false;
}

bool UIRoot::on_mouse_up(float x, float y, int button) {
    if (m_capturedWidget) {
        bool consumed = m_capturedWidget->on_mouse_up(x, y, button);
        set_capture(m_capturedWidget, false);
        return consumed;
    }
    UIWidget* target = find_widget_at(x, y);
    if (target) {
        return target->on_mouse_up(x, y, button);
    }
    return false;
}

bool UIRoot::on_mouse_move(float x, float y) {
    if (m_capturedWidget) {
        return m_capturedWidget->on_mouse_move(x, y);
    }
    UIWidget* target = find_widget_at(x, y);
    if (target != m_hoveredWidget) {
        m_hoveredWidget = target;
        if (target) {
            const UIRect& r = target->get_rect();
            LOG_INFO("HOVER: %s at (%f, %f) rect(%.1f, %.1f, %.1f, %.1f)",
                typeid(*target).name(), x, y, r.x, r.y, r.w, r.h);
        } else {
            LOG_INFO("HOVER: None at (%f, %f)", x, y);
        }
    }
    if (target) {
        return target->on_mouse_move(x, y);
    }
    return false;
}

bool UIRoot::on_key_down(int key, bool ctrl, bool shift) {
    return false;
}

bool UIRoot::on_char(char c) {
    return false;
}

void UIRoot::set_capture(UIWidget* widget, bool capture) {
    if (capture) {
        m_capturedWidget = widget;
    } else {
        if (m_capturedWidget == widget) {
            m_capturedWidget = nullptr;
        }
    }
}