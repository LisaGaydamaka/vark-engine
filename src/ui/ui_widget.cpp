#include "ui_widget.h"
#include "ui_root.h"
#include "ui_renderer.h"
#include "core/logger.h"

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
    // ---- Self clipping ----
    bool selfClip = clips_self();
    if (selfClip) {
        ui->push_clip_rect(m_rect.x, m_rect.y, m_rect.w, m_rect.h);
    }
    render(ui);  // draw own content
    if (selfClip) {
        ui->pop_clip_rect();
    }

    // ---- Child clipping (if this widget clips children) ----
    bool childClip = clips_children();
    if (childClip) {
        ui->push_clip_rect(m_rect.x, m_rect.y, m_rect.w, m_rect.h);
    }
    for (auto& child : m_children) {
        child->render_all(ui);
    }
    if (childClip) {
        ui->pop_clip_rect();
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