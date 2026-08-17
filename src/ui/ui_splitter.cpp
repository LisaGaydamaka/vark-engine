#include "ui_splitter.h"
#include "ui_root.h"
#include "ui_renderer.h"   // <-- ADD THIS
#include <algorithm>

UISplitter::UISplitter(Orientation orient, float initialRatio)
    : m_orientation(orient), m_ratio(initialRatio) {}

void UISplitter::layout() {
    if (m_children.size() != 2) return;

    UIWidget* first = m_children[0].get();
    UIWidget* second = m_children[1].get();

    float x = m_rect.x;
    float y = m_rect.y;
    float w = m_rect.w;
    float h = m_rect.h;

    if (m_orientation == Vertical) {
        float firstW = w * m_ratio;
        float secondW = w - firstW - m_handleThickness;
        first->set_rect(x, y, firstW, h);
        second->set_rect(x + firstW + m_handleThickness, y, secondW, h);
    } else {
        float firstH = h * m_ratio;
        float secondH = h - firstH - m_handleThickness;
        first->set_rect(x, y, w, firstH);
        second->set_rect(x, y + firstH + m_handleThickness, w, secondH);
    }

    if (first->get_rect().w < 0) first->set_rect(x, y, 0, h);
    if (second->get_rect().w < 0) second->set_rect(x + w - m_handleThickness, y, 0, h);
}

void UISplitter::render(UIRenderer* ui) {
    if (m_children.size() != 2) return;

    UIWidget* first = m_children[0].get();

    float x, y, w, h;
    if (m_orientation == Vertical) {
        x = first->get_rect().x + first->get_rect().w;
        y = m_rect.y;
        w = m_handleThickness;
        h = m_rect.h;
    } else {
        x = m_rect.x;
        y = first->get_rect().y + first->get_rect().h;
        w = m_rect.w;
        h = m_handleThickness;
    }

    ui->draw_rect(x, y, w, h, 0.4f, 0.4f, 0.4f, 1.0f);
    ui->draw_rect(x, y, w, 1, 0.5f, 0.5f, 0.5f, 1.0f);
    ui->draw_rect(x, y + h - 1, w, 1, 0.3f, 0.3f, 0.3f, 1.0f);
    if (m_orientation == Vertical) {
        ui->draw_rect(x, y, 1, h, 0.5f, 0.5f, 0.5f, 1.0f);
        ui->draw_rect(x + w - 1, y, 1, h, 0.3f, 0.3f, 0.3f, 1.0f);
    } else {
        ui->draw_rect(x, y, w, 1, 0.5f, 0.5f, 0.5f, 1.0f);
        ui->draw_rect(x, y + h - 1, w, 1, 0.3f, 0.3f, 0.3f, 1.0f);
    }
}

bool UISplitter::is_on_handle(float x, float y) const {
    if (m_children.size() != 2) return false;

    UIWidget* first = m_children[0].get();
    float handleX, handleY, handleW, handleH;
    if (m_orientation == Vertical) {
        handleX = first->get_rect().x + first->get_rect().w;
        handleY = m_rect.y;
        handleW = m_handleThickness;
        handleH = m_rect.h;
    } else {
        handleX = m_rect.x;
        handleY = first->get_rect().y + first->get_rect().h;
        handleW = m_rect.w;
        handleH = m_handleThickness;
    }
    return x >= handleX && x <= handleX + handleW &&
           y >= handleY && y <= handleY + handleH;
}

void UISplitter::update_ratio_from_mouse(float x, float y) {
    if (m_children.size() != 2) return;

    float newRatio = m_ratio;
    if (m_orientation == Vertical) {
        float totalW = m_rect.w - m_handleThickness;
        if (totalW <= 0) return;
        float mouseX = x - m_rect.x;
        newRatio = mouseX / totalW;
    } else {
        float totalH = m_rect.h - m_handleThickness;
        if (totalH <= 0) return;
        float mouseY = y - m_rect.y;
        newRatio = mouseY / totalH;
    }
    newRatio = std::max(0.05f, std::min(0.95f, newRatio));
    m_ratio = newRatio;
}

bool UISplitter::on_mouse_down(float x, float y, int button) {
    if (button == 0 && is_on_handle(x, y)) {
        m_dragging = true;
        UIRoot* root = get_root();
        if (root) {
            root->set_capture(this, true);
        }
        return true;
    }
    return false;
}

bool UISplitter::on_mouse_up(float x, float y, int button) {
    if (button == 0 && m_dragging) {
        m_dragging = false;
        UIRoot* root = get_root();
        if (root) {
            root->set_capture(this, false);
        }
        return true;
    }
    return false;
}

bool UISplitter::on_mouse_move(float x, float y) {
    if (m_dragging) {
        update_ratio_from_mouse(x, y);
        return true;
    }
    return false;
}