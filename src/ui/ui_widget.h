#pragma once
#include <vector>
#include <memory>
#include <functional>

class UIRenderer;
class UIRoot;

struct UIRect {
    float x, y, w, h;
    bool contains(float px, float py) const {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

class UIWidget {
public:
    UIWidget() = default;
    virtual ~UIWidget() = default;

    void set_rect(float x, float y, float w, float h) { m_rect = {x, y, w, h}; }
    const UIRect& get_rect() const { return m_rect; }

    UIWidget* get_parent() const { return m_parent; }
    void add_child(std::unique_ptr<UIWidget> child);
    const std::vector<std::unique_ptr<UIWidget>>& get_children() const { return m_children; }

    UIRoot* get_root();

    virtual void render(UIRenderer* ui) {}
    virtual void layout() {}
    virtual bool on_mouse_down(float x, float y, int button) { return false; }
    virtual bool on_mouse_up(float x, float y, int button) { return false; }
    virtual bool on_mouse_move(float x, float y) { return false; }
    virtual bool on_key_down(int key, bool ctrl, bool shift) { return false; }
    virtual bool on_char(char c) { return false; }

    virtual bool hit_test(float x, float y) const { return m_rect.contains(x, y); }
    virtual bool is_priority_hit(float x, float y) const { return false; }

    virtual void render_all(UIRenderer* ui);
    virtual void layout_all();

    // ---- Focus management ----
    void request_focus();   // call this to ask for focus
    void set_focus(bool focused);  // called by root
    virtual void on_focus_gained() {}
    virtual void on_focus_lost() {}

    bool is_focused() const { return m_focused; }

protected:
    UIRect m_rect = {0, 0, 0, 0};
    UIWidget* m_parent = nullptr;
    std::vector<std::unique_ptr<UIWidget>> m_children;
    bool m_focused = false;
};