#pragma once
#include "ui_widget.h"
#include <memory>

class UILayout {
public:
    virtual ~UILayout() = default;
    virtual void apply(UIWidget* parent) = 0;
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