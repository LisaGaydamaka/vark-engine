// src/editor/editor_ui.h
#pragma once
#include "core/math.h"
#include "ui/ui_renderer.h"
#include "world/level.h"
#include <vector>
#include <string>

class Editor; // forward declaration

class EditorUI {
public:
    enum class EditField {
        None,
        PosX, PosY, PosZ,
        SizeX, SizeY, SizeZ
    };

    EditorUI(Editor* editor, UIRenderer* ui);
    ~EditorUI() = default;

    void render();
    void set_brushes(const std::vector<Brush>& brushes);
    void set_selected(int index);

    // Returns true if the event was consumed by the UI
    bool on_mouse_click(int x, int y);
    bool on_key_down(int key);
    bool on_text_input(char c);

private:
    // Drawing helpers
    void draw_top_menu();
    void draw_brush_list();
    void draw_inspector();

    // Layout constants
    static constexpr int TOP_MENU_HEIGHT = 30;
    static constexpr int LEFT_PANEL_WIDTH = 220;
    static constexpr int RIGHT_PANEL_WIDTH = 280;

    // Click detection helpers
    bool hit_test_rect(int x, int y, float rx, float ry, float rw, float rh) const;

    Editor* m_editor;
    UIRenderer* m_ui;

    const std::vector<Brush>* m_brushes = nullptr;
    std::vector<int> m_sortedIndices;   // indices sorted by brush.time
    int m_selectedIndex = -1;           // original index in m_brushes

    // Edit state
    EditField m_editField = EditField::None;
    std::string m_editBuffer;

    // Menu state (simple)
    enum class Menu { None, File, Edit, Build };
    Menu m_activeMenu = Menu::None;
};