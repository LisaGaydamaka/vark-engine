#pragma once
#include "ui_widget.h"
#include <string>
#include <functional>

class UITextField : public UIWidget {
public:
    UITextField();
    ~UITextField() = default;

    void set_text(const std::string& text);
    const std::string& get_text() const { return m_text; }

    void set_placeholder(const std::string& text) { m_placeholder = text; }
    void set_commit_callback(std::function<void(const std::string&)> callback) { m_onCommit = callback; }
    void set_cancel_callback(std::function<void()> callback) { m_onCancel = callback; }

    void render(UIRenderer* ui) override;
    bool on_mouse_down(float x, float y, int button) override;
    bool on_mouse_up(float x, float y, int button) override;
    bool on_mouse_move(float x, float y) override;
    bool on_key_down(int key, bool ctrl, bool shift) override;
    bool on_char(char c) override;

    void on_focus_gained() override { m_cursorVisible = true; }
    void on_focus_lost() override { m_selectionStart = m_selectionEnd = m_cursorPos; }

private:
    std::string m_text;
    std::string m_placeholder;
    int m_cursorPos = 0;
    int m_selectionStart = 0;   // start of selection (inclusive)
    int m_selectionEnd = 0;     // end of selection (exclusive)
    float m_blinkTimer = 0.0f;
    bool m_cursorVisible = true;
    bool m_dragging = false;

    std::function<void(const std::string&)> m_onCommit;
    std::function<void()> m_onCancel;

    void commit();
    void cancel();
    void insert_char(char c);
    void delete_char();
    void move_cursor(int delta, bool extendSelection);
    void move_to_home(bool extendSelection);
    void move_to_end(bool extendSelection);
    void select_all();
    void copy_to_clipboard();
    void cut_to_clipboard();
    void paste_from_clipboard();

    // Helper: get the selected text range (normalized)
    void get_selection(int& start, int& end) const;
    void delete_selection();
    void replace_selection(const std::string& text);
};