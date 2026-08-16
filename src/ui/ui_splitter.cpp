#include "ui_splitter.h"
#include "ui_renderer.h"
#include "../common/core/logger.h"
#include <windows.h>
#include <algorithm>

extern HWND g_hwnd;  // declared in editor_main.cpp

UISplitter::UISplitter(Orientation orient)
    : m_orientation(orient) {}

void UISplitter::set_ratio(float ratio) {
    m_ratio = std::max(0.0f, std::min(1.0f, ratio));
    update_child_rects();
}

void UISplitter::layout() {
    if (m_children.size() < 2) {
        if (m_children.size() == 1) {
            m_children[0]->set_rect(m_rect.x, m_rect.y, m_rect.w, m_rect.h);
        }
        return;
    }
    update_child_rects();
    for (auto& child : m_children) {
        child->layout();  // let children layout themselves
    }
}

void UISplitter::update_child_rects() {
    if (m_children.size() < 2) return;
    float x = m_rect.x;
    float y = m_rect.y;
    float w = m_rect.w;
    float h = m_rect.h;
    float handle = m_handleSize;

    UIWidget* first = m_children[0].get();
    UIWidget* second = m_children[1].get();

    if (m_orientation == Orientation::Vertical) {
        float firstW = (w - handle) * m_ratio;
        float secondW = w - handle - firstW;
        first->set_rect(x, y, firstW, h);
        second->set_rect(x + firstW + handle, y, secondW, h);
    } else {
        float firstH = (h - handle) * m_ratio;
        float secondH = h - handle - firstH;
        first->set_rect(x, y, w, firstH);
        second->set_rect(x, y + firstH + handle, w, secondH);
    }
}

void UISplitter::render(UIRenderer* ui) {
    float x = m_rect.x;
    float y = m_rect.y;
    float w = m_rect.w;
    float h = m_rect.h;
    float handle = m_handleSize;

    if (m_orientation == Orientation::Vertical) {
        float handleX = x + (w - handle) * m_ratio;
        ui->draw_rect(handleX, y, handle, h, 0.3f, 0.3f, 0.3f, 1.0f);
        ui->draw_rect(handleX, y, 1.0f, h, 0.5f, 0.5f, 0.5f, 1.0f);
        ui->draw_rect(handleX + handle - 1.0f, y, 1.0f, h, 0.5f, 0.5f, 0.5f, 1.0f);
    } else {
        float handleY = y + (h - handle) * m_ratio;
        ui->draw_rect(x, handleY, w, handle, 0.3f, 0.3f, 0.3f, 1.0f);
        ui->draw_rect(x, handleY, w, 1.0f, 0.5f, 0.5f, 0.5f, 1.0f);
        ui->draw_rect(x, handleY + handle - 1.0f, w, 1.0f, 0.5f, 0.5f, 0.5f, 1.0f);
    }
}

bool UISplitter::on_mouse_down(float x, float y, int button) {
    if (button != 0) return false;

    float handleX, handleY, handleW, handleH;
    if (m_orientation == Orientation::Vertical) {
        handleX = m_rect.x + (m_rect.w - m_handleSize) * m_ratio;
        handleY = m_rect.y;
        handleW = m_handleSize;
        handleH = m_rect.h;
    } else {
        handleX = m_rect.x;
        handleY = m_rect.y + (m_rect.h - m_handleSize) * m_ratio;
        handleW = m_rect.w;
        handleH = m_handleSize;
    }
    if (x >= handleX && x <= handleX + handleW &&
        y >= handleY && y <= handleY + handleH) {
        m_dragging = true;
        SetCapture(g_hwnd);
        return true;
    }
    return false;
}

bool UISplitter::on_mouse_up(float x, float y, int button) {
    if (button == 0 && m_dragging) {
        m_dragging = false;
        ReleaseCapture();
        return true;
    }
    return false;
}

bool UISplitter::on_mouse_move(float x, float y) {
    if (!m_dragging) return false;

    if (m_orientation == Orientation::Vertical) {
        float relX = x - m_rect.x;
        float maxX = m_rect.w - m_handleSize;
        if (maxX > 0) {
            m_ratio = std::max(0.0f, std::min(1.0f, relX / maxX));
        }
    } else {
        float relY = y - m_rect.y;
        float maxY = m_rect.h - m_handleSize;
        if (maxY > 0) {
            m_ratio = std::max(0.0f, std::min(1.0f, relY / maxY));
        }
    }
    update_child_rects();
    if (m_onResize) {
        m_onResize(m_ratio);
    }
    return true;
}