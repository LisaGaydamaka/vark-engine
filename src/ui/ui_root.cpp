#include "ui_root.h"
#include "../common/core/logger.h"
#include <cstdio>
#include <typeinfo>

static UIWidget* find_deepest_widget(UIWidget* parent, float x, float y) {
    if (parent->is_priority_hit(x, y)) {
        LOG_INFO("UI: priority hit on %s", typeid(*parent).name());
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
    LOG_INFO("UI: on_mouse_down(%f, %f, %d)", x, y, button);
    if (m_capturedWidget) {
        m_capturedWidget->on_mouse_down(x, y, button);
        return true;
    }
    UIWidget* target = find_widget_at(x, y);
    if (target) {
        LOG_INFO("UI: target found: %s", typeid(*target).name());
        if (m_focusedWidget && target != m_focusedWidget) {
            LOG_INFO("UI: target different from focused, unfocusing");
            set_focused_widget(nullptr);
        }
        bool consumed = target->on_mouse_down(x, y, button);
        return consumed;
    } else {
        LOG_INFO("UI: no target found, unfocusing");
        if (m_focusedWidget) {
            set_focused_widget(nullptr);
        }
        return false;
    }
}

bool UIRoot::on_mouse_up(float x, float y, int button) {
    if (m_capturedWidget) {
        LOG_INFO("UI: on_mouse_up delivered to captured widget %s", typeid(*m_capturedWidget).name());
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
        m_capturedWidget->on_mouse_move(x, y);
        return true;
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
        target->on_mouse_move(x, y);
    }
    return false;
}

bool UIRoot::on_key_down(int key, bool ctrl, bool shift) {
    LOG_INFO("UI: on_key_down(%d, %d, %d)", key, ctrl, shift);
    if (m_focusedWidget) {
        LOG_INFO("UI: forwarding to focused widget: %s", typeid(*m_focusedWidget).name());
        return m_focusedWidget->on_key_down(key, ctrl, shift);
    }
    LOG_WARN("UI: no focused widget for key down");
    return false;
}

bool UIRoot::on_char(char c) {
    LOG_INFO("UI: on_char('%c')", c);
    if (m_focusedWidget) {
        LOG_INFO("UI: forwarding to focused widget: %s", typeid(*m_focusedWidget).name());
        return m_focusedWidget->on_char(c);
    }
    LOG_WARN("UI: no focused widget for char");
    return false;
}

void UIRoot::set_capture(UIWidget* widget, bool capture) {
    if (capture) {
        m_capturedWidget = widget;
        LOG_INFO("UI: capture set to %s", widget ? typeid(*widget).name() : "null");
    } else {
        if (m_capturedWidget == widget) {
            m_capturedWidget = nullptr;
            LOG_INFO("UI: capture released");
        }
    }
}

void UIRoot::set_focused_widget(UIWidget* widget) {
    LOG_INFO("UI: set_focused_widget called with %s", widget ? typeid(*widget).name() : "null");
    if (m_focusedWidget == widget) {
        LOG_INFO("UI: already focused, returning");
        return;
    }
    if (m_focusedWidget) {
        LOG_INFO("UI: unfocusing old widget");
        m_focusedWidget->set_focus(false);
    }
    m_focusedWidget = widget;
    if (widget) {
        LOG_INFO("UI: focusing new widget");
        widget->set_focus(true);
    }
}

// ---- NEW: Mouse wheel handling with bubble-up ----
bool UIRoot::on_mouse_wheel(float delta, float x, float y) {
    UIWidget* target = find_widget_at(x, y);
    while (target) {
        if (target->on_mouse_wheel(delta, x, y))
            return true;
        target = target->get_parent();
    }
    return false;
}