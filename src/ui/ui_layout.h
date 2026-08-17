#pragma once
#include "ui_widget.h"
#include <memory>

class UILayout {
public:
    virtual ~UILayout() = default;
    virtual void apply(UIWidget* parent) = 0;

    void set_padding(float left, float top, float right, float bottom) {
        m_paddingLeft = left; m_paddingTop = top; m_paddingRight = right; m_paddingBottom = bottom;
    }

protected:
    float m_paddingLeft = 0.0f;
    float m_paddingTop = 0.0f;
    float m_paddingRight = 0.0f;
    float m_paddingBottom = 0.0f;
};

class UIVBoxLayout : public UILayout {
public:
    UIVBoxLayout(float spacing = 4.0f) : m_spacing(spacing) {}
    void apply(UIWidget* parent) override;
private:
    float m_spacing;
};

class UIHBoxLayout : public UILayout {
public:
    UIHBoxLayout(float spacing = 4.0f) : m_spacing(spacing) {}
    void apply(UIWidget* parent) override;
private:
    float m_spacing;
};

class UIFillLayout : public UILayout {
public:
    void apply(UIWidget* parent) override;
};