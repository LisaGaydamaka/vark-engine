#include "ui_widget.h"
#include "ui_root.h"

void UIWidget::add_child(std::unique_ptr<UIWidget> child) {
    if (child) {
        child->m_parent = this;
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