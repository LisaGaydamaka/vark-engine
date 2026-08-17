#include "ui_widget.h"
#include "ui_root.h"
#include "core/logger.h"

// ---- NEW: destructor that notifies root ----
UIWidget::~UIWidget() {
    if (auto root = get_root()) {
        root->notify_widget_destroyed(this);
    }
}

void UIWidget::add_child(std::unique_ptr<UIWidget> child) {
    if (child) {
        child->set_parent(this);
        m_children.push_back(std::move(child));
    }
}

void UIWidget::render_all(UIRenderer* ui) {
    render(ui);
    for (auto& child : m_children) {
        child->render_all(ui);
    }
}

void UIWidget::layout_all() {
    layout();
    for (auto& child : m_children) {
        child->layout_all();
    }
}

UIRoot* UIWidget::get_root() {
    UIWidget* current = this;
    while (current) {
        UIRoot* root = dynamic_cast<UIRoot*>(current);
        if (root) return root;
        current = current->m_parent;
    }
    return nullptr;
}

void UIWidget::request_focus() {
    LOG_INFO("UIWidget::request_focus() for %s", typeid(*this).name());
    UIRoot* root = get_root();
    if (root) {
        root->set_focused_widget(this);
    } else {
        LOG_WARN("UIWidget::request_focus: no root, setting local focus only");
        set_focus(true);
    }
}

void UIWidget::set_focus(bool focused) {
    LOG_INFO("UIWidget::set_focus(%d) for %s", focused, typeid(*this).name());
    if (m_focused == focused) return;
    m_focused = focused;
    if (focused) {
        on_focus_gained();
    } else {
        on_focus_lost();
    }
}