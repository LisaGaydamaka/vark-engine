// src/editor/editor.h
#pragma once
#include "core/math.h"
#include "world/level.h"
#include "ui/ui_renderer.h"
#include "editor_camera.h"
#include "editor_settings.h"
#include "editor_ui.h"
#include <vector>
#include <memory>
#include <wrl/client.h>

class Editor {
public:
    Editor();
    ~Editor();

    bool initialize(Renderer* renderer, Level* level, UIRenderer* ui);
    void shutdown();
    void update(float dt);
    void render();

    // Brush management
    void sync_brushes();
    void save_level();
    void delete_selected();
    void add_brush(BrushType type, ShapeType shape);
    void select_brush(int index);
    int pick_brush(int mouseX, int mouseY);

    // Camera access
    Camera* get_camera() { return m_editorCamera.get_camera(); }
    const Camera* get_camera() const { return m_editorCamera.get_camera(); }

    // Settings
    void set_keybinds(const EditorKeybindSettings& keybinds);

    // UI access
    EditorUI* get_ui() { return m_uiHandler.get(); }

    // Input events
    void on_mouse_move(int dx, int dy, bool leftDown, bool middleDown, bool rightDown, int modMask);
    void on_mouse_button(int button, bool pressed);
    void on_mouse_wheel(int delta);
    void on_key_down(int key, bool ctrl, bool shift);
    void on_key_up(int key);

    // Edit callbacks from UI
    void apply_brush_edit(int field, float value);
    void cancel_brush_edit();

private:
    void rebuild_wireframe_buffer();

    // Raycasting helpers
    Vec3 screen_to_world_ray(int mouseX, int mouseY); // removed const
    bool intersect_aabb(const Vec3& rayOrigin, const Vec3& rayDir, const Vec3& boxMin, const Vec3& boxMax, float& outT) const;

    Renderer* m_renderer = nullptr;
    Level* m_level = nullptr;
    UIRenderer* m_ui = nullptr;

    std::vector<Brush> m_brushes;
    int m_selectedIndex = -1;

    EditorCamera m_editorCamera;
    EditorKeybindSettings m_keybinds;

    std::unique_ptr<EditorUI> m_uiHandler;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_wireframeBuffer;
    int m_wireframeVertexCount = 0;

    bool m_initialized = false;
};