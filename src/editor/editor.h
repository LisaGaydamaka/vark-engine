// editor/editor.h
#pragma once
#include "core/math.h"
#include "world/level.h"
#include "editor_camera.h"
#include "editor_settings.h"
#include <vector>
#include <memory>
#include <wrl/client.h>

class Editor {
public:
    Editor();
    ~Editor();

    bool initialize(Renderer* renderer, Level* level);
    void shutdown();
    void update(float dt);
    void render();

    void sync_brushes();
    void save_level();
    void delete_selected();
    void add_brush(BrushType type, ShapeType shape);
    void select_brush(int index);
    int pick_brush(int mouseX, int mouseY);

    Camera* get_camera() { return m_editorCamera.get_camera(); }
    const Camera* get_camera() const { return m_editorCamera.get_camera(); }

    void set_keybinds(const EditorKeybindSettings& keybinds);

    // ---- Data access for UI ----
    const std::vector<Brush>& get_brushes() const { return m_brushes; }
    std::vector<Brush>& get_brushes_mutable() { return m_brushes; }   // <-- NEW
    int get_selected_index() const { return m_selectedIndex; }

    void apply_brush_edit(int field, float value);

    void on_mouse_move(int dx, int dy, bool leftDown, bool middleDown, bool rightDown, int modMask);
    void on_mouse_wheel(int delta);
    void on_key_down(int key, bool ctrl, bool shift);

    // ---- NEW: renumber times ----
    void refresh_times();

private:
    void rebuild_wireframe_buffer();

    Vec3 screen_to_world_ray(int mouseX, int mouseY);
    bool intersect_aabb(const Vec3& rayOrigin, const Vec3& rayDir, const Vec3& boxMin, const Vec3& boxMax, float& outT) const;

    Renderer* m_renderer = nullptr;
    Level* m_level = nullptr;

    std::vector<Brush> m_brushes;
    int m_selectedIndex = -1;

    EditorCamera m_editorCamera;
    EditorKeybindSettings m_keybinds;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_wireframeBuffer;
    int m_wireframeVertexCount = 0;

    bool m_initialized = false;
};