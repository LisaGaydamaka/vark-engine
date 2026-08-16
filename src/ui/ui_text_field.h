#pragma once
#include "ui_widget.h"
#include <string>
#include <functional>

class UITextField : public UIWidget {
public:
    UITextField(float value = 0.0f, std::function<void(float)> onCommit = nullptr);
    void set_value(float v);
    float get_value() const;

    void render(UIRenderer* ui) override;
    bool on_mouse_down(int x, int y, int button) override;
    bool on_key_down(int key, bool ctrl, bool shift) override;
    bool on_text_input(char c) override;

    void set_on_commit(std::function<void(float)> cb) { m_onCommit = cb; }

private:
    float m_value = 0.0f;
    std::string m_text;
    bool m_focused = false;
    std::function<void(float)> m_onCommit;
    void commit();
};