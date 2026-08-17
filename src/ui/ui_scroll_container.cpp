#include "ui_scroll_container.h"
#include "ui_renderer.h"
#include "ui_root.h"
#include "core/logger.h"
#include "ui_style.h"
#include <algorithm>

UIScrollContainer::UIScrollContainer() {
    m_scrollbarWidth = UIStyle::scrollbarWidth;
    auto sb = std::make_unique<UIScrollBar>(UIScrollBar::Orientation::Vertical);
    m_scrollbar = sb.get();
    m_scrollbar->set_parent(this);
    add_child(std::move(sb));

    m_scrollbar->set_on_value_changed([this](float val) {
        on_scrollbar_value_changed(val);
    });
}

void UIScrollContainer::set_child(std::unique_ptr<UIWidget> child) {
    if (!child) {
        m_child = nullptr;
        m_scrollable = nullptr;
        return;
    }

    // Check if the child implements IScrollable.
    IScrollable* scrollable = dynamic_cast<IScrollable*>(child.get());
    if (!scrollable) {
        LOG_ERROR("UIScrollContainer: child does not implement IScrollable; ignoring.");
        return;
    }

    child->set_parent(this);
    m_child = child.get();
    m_scrollable = scrollable;
    add_child(std::move(child));
}

void UIScrollContainer::layout() {
    if (!m_child || !m_scrollable) return;

    m_contentHeight = m_scrollable->get_content_height();
    m_viewportHeight = m_rect.h;

    float childWidth = m_rect.w - m_scrollbarWidth;
    if (childWidth < 0) childWidth = 0;
    m_child->set_rect(m_rect.x, m_rect.y, childWidth, m_viewportHeight);
    m_child->layout();

    float sbX = m_rect.x + m_rect.w - m_scrollbarWidth;
    m_scrollbar->set_rect(sbX, m_rect.y, m_scrollbarWidth, m_rect.h);
    m_scrollbar->layout();

    update_scrollbar();
}

void UIScrollContainer::render(UIRenderer* ui) {
    if (!m_child) return;

    ui->push_clip_rect(m_rect.x, m_rect.y, m_rect.w - m_scrollbarWidth, m_rect.h);
    m_child->render_all(ui);
    ui->pop_clip_rect();

    m_scrollbar->render_all(ui);
}

bool UIScrollContainer::on_mouse_wheel(float delta, float x, float y) {
    (void)x; (void)y;
    if (!m_scrollable) return false;

    float step = 60.0f;
    float maxOffset = std::max(0.0f, m_contentHeight - m_viewportHeight);
    float newOffset = m_scrollOffset - delta * step;
    newOffset = std::clamp(newOffset, 0.0f, maxOffset);
    if (newOffset != m_scrollOffset) {
        m_scrollOffset = newOffset;
        m_scrollable->set_scroll_offset(m_scrollOffset);
        update_scrollbar();
        return true;
    }
    return false;
}

bool UIScrollContainer::on_mouse_down(float x, float y, int button) {
    if (m_scrollbar && m_scrollbar->hit_test(x, y)) {
        return m_scrollbar->on_mouse_down(x, y, button);
    }
    if (m_child && m_child->hit_test(x, y)) {
        return m_child->on_mouse_down(x, y, button);
    }
    return false;
}

bool UIScrollContainer::on_mouse_up(float x, float y, int button) {
    if (m_scrollbar && m_scrollbar->hit_test(x, y)) {
        return m_scrollbar->on_mouse_up(x, y, button);
    }
    if (m_child && m_child->hit_test(x, y)) {
        return m_child->on_mouse_up(x, y, button);
    }
    return false;
}

bool UIScrollContainer::on_mouse_move(float x, float y) {
    if (m_scrollbar && m_scrollbar->hit_test(x, y)) {
        return m_scrollbar->on_mouse_move(x, y);
    }
    if (m_child && m_child->hit_test(x, y)) {
        return m_child->on_mouse_move(x, y);
    }
    return false;
}

void UIScrollContainer::update_scrollbar() {
    if (!m_scrollable) {
        LOG_WARN("update_scrollbar called with null scrollable");
        return;
    }
    m_contentHeight = m_scrollable->get_content_height();
    m_viewportHeight = m_rect.h;
    float maxOffset = std::max(0.0f, m_contentHeight - m_viewportHeight);
    m_scrollOffset = std::clamp(m_scrollOffset, 0.0f, maxOffset);

    m_scrollbar->set_range(0.0f, maxOffset);
    m_scrollbar->set_value(m_scrollOffset);

    // The scrollable already has its offset set via set_scroll_offset when wheel/bar changes.
    // But in case the bar was moved programmatically, we propagate here.
    m_scrollable->set_scroll_offset(m_scrollOffset);
}

void UIScrollContainer::on_scrollbar_value_changed(float value) {
    m_scrollOffset = value;
    if (m_scrollable) {
        m_scrollable->set_scroll_offset(m_scrollOffset);
    }
}