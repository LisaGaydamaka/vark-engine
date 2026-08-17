#include "ui_scrollbar.h"
#include "ui_renderer.h"
#include "ui_root.h"
#include "core/logger.h"
#include "ui_style.h"
#include <algorithm>

UIScrollBar::UIScrollBar(Orientation orient) : m_orientation(orient) {}

void UIScrollBar::set_range(float min, float max) {
    m_min = min;
    m_max = max;
    if (m_value < m_min) m_value = m_min;
    if (m_value > m_max) m_value = m_max;
    update_thumb_size();
}

void UIScrollBar::set_value(float val) {
    float newVal = std::clamp(val, m_min, m_max);
    if (newVal != m_value) {
        m_value = newVal;
        if (m_onValueChanged) m_onValueChanged(m_value);
    }
}

float UIScrollBar::get_track_length() const {
    return (m_orientation == Orientation::Vertical) ? m_rect.h : m_rect.w;
}

float UIScrollBar::get_thumb_position() const {
    float trackLen = get_track_length();
    float thumbSize = m_thumbSize;
    if (trackLen <= thumbSize) return 0.0f;
    float range = m_max - m_min;
    if (range == 0.0f) return 0.0f;
    float t = (m_value - m_min) / range;
    return t * (trackLen - thumbSize);
}

// ---- NEW: compute thumb size from content vs viewport ----
void UIScrollBar::update_thumb_size() {
    float trackLen = get_track_length();
    float range = m_max - m_min;
    // If we have a viewport size, use it, otherwise fallback to old heuristic
    if (m_viewportSize > 0.0f && range > 0.0f) {
        float contentSize = range + m_viewportSize; // range is max-offset, so content = viewport + range
        float ratio = m_viewportSize / contentSize;
        m_thumbSize = trackLen * ratio;
        // Clamp to reasonable size
        m_thumbSize = std::clamp(m_thumbSize, 10.0f, trackLen);
    } else {
        // Fallback: old heuristic
        if (range == 0.0f) {
            m_thumbSize = trackLen;
        } else {
            float size = trackLen * (1.0f / (1.0f + range / 100.0f));
            m_thumbSize = std::clamp(size, 20.0f, trackLen);
        }
    }
}

void UIScrollBar::render(UIRenderer* ui) {
    ui->draw_rect(m_rect.x, m_rect.y, m_rect.w, m_rect.h,
                  UIStyle::scrollbarTrackR, UIStyle::scrollbarTrackG, UIStyle::scrollbarTrackB, UIStyle::scrollbarTrackA);

    if (m_thumbSize > 0.0f) {
        float thumbPos = get_thumb_position();
        float thumbX, thumbY, thumbW, thumbH;
        if (m_orientation == Orientation::Vertical) {
            thumbX = m_rect.x + 2.0f;
            thumbY = m_rect.y + thumbPos;
            thumbW = m_rect.w - 4.0f;
            thumbH = m_thumbSize;
        } else {
            thumbX = m_rect.x + thumbPos;
            thumbY = m_rect.y + 2.0f;
            thumbW = m_thumbSize;
            thumbH = m_rect.h - 4.0f;
        }
        ui->draw_rect(thumbX, thumbY, thumbW, thumbH,
                      UIStyle::scrollbarThumbR, UIStyle::scrollbarThumbG, UIStyle::scrollbarThumbB, UIStyle::scrollbarThumbA);
        ui->draw_rect(thumbX, thumbY, thumbW, 1,
                      UIStyle::scrollbarThumbBorderLightR, UIStyle::scrollbarThumbBorderLightG, UIStyle::scrollbarThumbBorderLightB, UIStyle::scrollbarThumbBorderLightA);
        ui->draw_rect(thumbX, thumbY + thumbH - 1, thumbW, 1,
                      UIStyle::scrollbarThumbBorderDarkR, UIStyle::scrollbarThumbBorderDarkG, UIStyle::scrollbarThumbBorderDarkB, UIStyle::scrollbarThumbBorderDarkA);
        ui->draw_rect(thumbX, thumbY, 1, thumbH,
                      UIStyle::scrollbarThumbBorderLightR, UIStyle::scrollbarThumbBorderLightG, UIStyle::scrollbarThumbBorderLightB, UIStyle::scrollbarThumbBorderLightA);
        ui->draw_rect(thumbX + thumbW - 1, thumbY, 1, thumbH,
                      UIStyle::scrollbarThumbBorderDarkR, UIStyle::scrollbarThumbBorderDarkG, UIStyle::scrollbarThumbBorderDarkB, UIStyle::scrollbarThumbBorderDarkA);
    }
}


bool UIScrollBar::on_mouse_down(float x, float y, int button) {
    if (button != 0) return false;
    LOG_INFO("ScrollBar::on_mouse_down(%.1f, %.1f)", x, y);

    float thumbPos = get_thumb_position();
    float thumbStart, thumbEnd;
    if (m_orientation == Orientation::Vertical) {
        thumbStart = m_rect.y + thumbPos;
        thumbEnd = thumbStart + m_thumbSize;
        if (y >= thumbStart && y <= thumbEnd) {
            m_dragging = true;
            m_dragOffset = y - thumbStart;
            UIRoot* root = get_root();
            if (root) {
                root->set_capture(this, true);
                LOG_INFO("ScrollBar: captured mouse");
            } else {
                LOG_WARN("ScrollBar: root is null, cannot capture");
            }
            return true;
        } else {
            float trackLen = get_track_length();
            float range = m_max - m_min;
            if (range > 0.0f) {
                float t = (y - m_rect.y) / trackLen;
                float newValue = m_min + t * range;
                set_value(newValue);
                return true;
            }
        }
    } else {
        thumbStart = m_rect.x + thumbPos;
        thumbEnd = thumbStart + m_thumbSize;
        if (x >= thumbStart && x <= thumbEnd) {
            m_dragging = true;
            m_dragOffset = x - thumbStart;
            UIRoot* root = get_root();
            if (root) {
                root->set_capture(this, true);
                LOG_INFO("ScrollBar: captured mouse");
            } else {
                LOG_WARN("ScrollBar: root is null, cannot capture");
            }
            return true;
        } else {
            float trackLen = get_track_length();
            float range = m_max - m_min;
            if (range > 0.0f) {
                float t = (x - m_rect.x) / trackLen;
                float newValue = m_min + t * range;
                set_value(newValue);
                return true;
            }
        }
    }
    return false;
}

bool UIScrollBar::on_mouse_up(float x, float y, int button) {
    LOG_INFO("ScrollBar::on_mouse_up(%.1f, %.1f, %d), m_dragging=%d", x, y, button, m_dragging);
    if (button == 0 && m_dragging) {
        m_dragging = false;
        UIRoot* root = get_root();
        if (root) {
            root->set_capture(this, false);
            LOG_INFO("ScrollBar: released capture");
        }
        return true;
    }
    if (m_dragging) {
        m_dragging = false;
        UIRoot* root = get_root();
        if (root) root->set_capture(this, false);
        LOG_INFO("ScrollBar: force reset dragging");
        return true;
    }
    return false;
}

bool UIScrollBar::on_mouse_move(float x, float y) {
    if (m_dragging) {
        float trackLen = get_track_length();
        float range = m_max - m_min;
        if (range == 0.0f || trackLen <= 0.0f) return true;
        float pos;
        if (m_orientation == Orientation::Vertical) {
            pos = y - m_rect.y - m_dragOffset;
        } else {
            pos = x - m_rect.x - m_dragOffset;
        }
        pos = std::clamp(pos, 0.0f, trackLen - m_thumbSize);
        float t = pos / (trackLen - m_thumbSize);
        float newValue = m_min + t * range;
        set_value(newValue);
        return true;
    }
    return false;
}