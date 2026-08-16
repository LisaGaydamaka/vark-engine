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
    

    void draw_brush_list();
    void draw_properties();
    void draw_edit_field(const char* label, float value, EditField field, float& y);

    bool is_point_in_rect(int x, int y, float rx, float ry, float rw, float rh) const;

    Editor* m_editor;
    UIRenderer* m_ui;
    const std::vector<Brush>* m_brushes = nullptr;
    int m_selectedIndex = -1;

    // Edit state
    EditField m_editField = EditField::None;
    std::string m_editBuffer;
};