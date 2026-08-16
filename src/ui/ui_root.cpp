#include "ui_root.h"
#include "../common/core/logger.h"
#include <cstdio>

static UIWidget* find_deepest_widget(UIWidget* parent, float x, float y) {
    const auto& children = parent->get_children();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        UIWidget* child = it->get();
        bool hit = child->hit_test(x, y);
        LOG_INFO("UI: Testing child at (%f,%f) rect(%f,%f,%f,%f) hit=%d", 
            x, y, child->get_rect().x, child->get_rect().y, 
            child->get_rect().w, child->get_rect().h, hit);
        if (hit) {
            UIWidget* deeper = find_deepest_widget(child, x, y);
            if (deeper) {
                LOG_INFO("UI: Found deeper widget (type: %s)", typeid(*deeper).name());
                return deeper;
            }
            LOG_INFO("UI: Returning child (type: %s)", typeid(*child).name());
            return child;
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
    UIWidget* target = find_widget_at(x, y);
    if (target) {
        LOG_INFO("UI: Target found, forwarding to %s", typeid(*target).name());
        bool consumed = target->on_mouse_down(x, y, button);
        LOG_INFO("UI: Event consumed = %d", consumed);
        return consumed;
    }
    LOG_WARN("UI: No target found for click at (%f, %f)", x, y);
    return false;
}

bool UIRoot::on_mouse_up(float x, float y, int button) {
    LOG_INFO("UI: on_mouse_up(%f, %f, %d)", x, y, button);
    UIWidget* target = find_widget_at(x, y);
    if (target) {
        LOG_INFO("UI: Target found, forwarding to %s", typeid(*target).name());
        bool consumed = target->on_mouse_up(x, y, button);
        LOG_INFO("UI: Event consumed = %d", consumed);
        return consumed;
    }
    return false;
}

bool UIRoot::on_mouse_move(float x, float y) {
    UIWidget* target = find_widget_at(x, y);
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