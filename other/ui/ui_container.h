#pragma once
#include "ui_widget.h"
#include "ui_layout.h"
#include <memory>

class UIContainer : public UIWidget {
public:
    UIContainer() = default;
    ~UIContainer() = default;

    void set_layout(std::unique_ptr<UILayout> layout) {
        m_layout = std::move(layout);
    }

    void layout() override {
        if (m_layout) {
            m_layout->apply(this);
        }
        for (auto& child : m_children) {
            child->layout();
        }
    }

    // ---- Enable clipping for children ----
    bool clips_children() const override { return true; }

private:
    std::unique_ptr<UILayout> m_layout;
};