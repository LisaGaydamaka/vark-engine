#include "ui_root.h"
#include "../common/core/logger.h"
#include <cstdio>
#include <typeinfo>

static UIWidget* find_deepest_widget(UIWidget* parent, float x, float y) {
    // Check priority hit first (e.g., splitter handle)
    if (parent->is_priority_hit(x, y)) {
        LOG_INFO("UI: priority hit on %s", typeid(*parent).name());
        return parent;
    }

    const auto& children = parent->get_children();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        UIWidget* child = it->get();
        if (child->hit_test(x, y)) {
            LOG_INFO("UI: child %s hit at (%f,%f)", typeid(*child).name(), x, y);
            UIWidget* deeper = find_deepest_widget(child, x, y);
            if (deeper) {
                LOG_INFO("UI: returning deeper %s", typeid(*deeper).name());
                return deeper;
            }
            LOG_INFO("UI: returning child %s", typeid(*child).name());
            return child;
        } else {
            LOG_INFO("UI: child %s miss at (%f,%f)", typeid(*child).name(), x, y);
        }
    }
    return nullptr;
}

UIWidget* UIRoot::find_widget_at(float x, float y) {
    LOG_INFO("UI: find_widget_at(%f, %f)", x, y);
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
        LOG_INFO("UI: captured widget %s gets event", typeid(*m_capturedWidget).name());
        m_capturedWidget->on_mouse_down(x, y, button);
        return true;
    }
    UIWidget* target = find_widget_at(x, y);
    if (target) {
        LOG_INFO("UI: Target found: %s", typeid(*target).name());
        bool consumed = target->on_mouse_down(x, y, button);
        LOG_INFO("UI: Event consumed = %d", consumed);
        return consumed;
    }
    LOG_WARN("UI: No target found for click at (%f, %f)", x, y);
    return false;
}

bool UIRoot::on_mouse_up(float x, float y, int button) {
    LOG_INFO("UI: on_mouse_up(%f, %f, %d)", x, y, button);
    if (m_capturedWidget) {
        bool consumed = m_capturedWidget->on_mouse_up(x, y, button);
        set_capture(m_capturedWidget, false);
        return consumed;
    }
    UIWidget* target = find_widget_at(x, y);
    if (target) {
        LOG_INFO("UI: Target found: %s", typeid(*target).name());
        bool consumed = target->on_mouse_up(x, y, button);
        LOG_INFO("UI: Event consumed = %d", consumed);
        return consumed;
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
    // Could forward to focused widget later
    return false;
}

bool UIRoot::on_char(char c) {
    return false;
}

void UIRoot::set_capture(UIWidget* widget, bool capture) {
    if (capture) {
        m_capturedWidget = widget;
        LOG_INFO("UI: capture set to %s", typeid(*widget).name());
    } else {
        if (m_capturedWidget == widget) {
            m_capturedWidget = nullptr;
            LOG_INFO("UI: capture released");
        }
    }
}