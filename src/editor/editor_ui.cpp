// src/editor/editor_ui.cpp
#include "editor_ui.h"
#include "editor.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

EditorUI::EditorUI(Editor* editor, UIRenderer* ui)
    : m_editor(editor), m_ui(ui) {
}

void EditorUI::set_brushes(const std::vector<Brush>& brushes) {
    m_brushes = &brushes;
    // Build sorted indices by brush.time (ascending)
    m_sortedIndices.clear();
    m_sortedIndices.reserve(brushes.size());
    for (size_t i = 0; i < brushes.size(); ++i)
        m_sortedIndices.push_back((int)i);
    std::sort(m_sortedIndices.begin(), m_sortedIndices.end(),
        [&](int a, int b) { return brushes[a].time < brushes[b].time; });
}

void EditorUI::set_selected(int index) {
    m_selectedIndex = index;
}

void EditorUI::render() {
    if (!m_ui) return;

    // 1. Top menu bar (always on top)
    draw_top_menu();

    // 2. Left panel: brush list
    draw_brush_list();

    // 3. Right panel: inspector
    draw_inspector();
}

// --------------------------------------------------------------------------
// Top menu bar
// --------------------------------------------------------------------------
void EditorUI::draw_top_menu() {
    const float y = 0.0f;
    const float height = TOP_MENU_HEIGHT;
    // Background bar
    m_ui->draw_rect(0, y, (float)m_ui->get_width(), height, 0.15f, 0.15f, 0.2f, 1.0f);

    float x = 10.0f;
    auto draw_menu_item = [&](const char* label, Menu menu) {
        bool active = (m_activeMenu == menu);
        float r = active ? 1.0f : 0.8f;
        float g = active ? 1.0f : 0.8f;
        float b = active ? 0.2f : 0.8f;
        m_ui->draw_text(x, y + 6.0f, label, r, g, b, 1.0f);
        x += 60.0f;
    };
    draw_menu_item("File", Menu::File);
    draw_menu_item("Edit", Menu::Edit);
    draw_menu_item("Build", Menu::Build);

    // Simple info text on the right
    char info[64];
    snprintf(info, sizeof(info), "Selected: %d", m_selectedIndex);
    m_ui->draw_text((float)m_ui->get_width() - 150.0f, y + 6.0f, info, 0.7f, 0.7f, 0.7f, 1.0f);
}

// --------------------------------------------------------------------------
// Left panel: brush list (sorted by time)
// --------------------------------------------------------------------------
void EditorUI::draw_brush_list() {
    const float x = 0.0f;
    const float y = TOP_MENU_HEIGHT;
    const float width = LEFT_PANEL_WIDTH;
    const float height = (float)m_ui->get_height() - y;

    // Panel background
    m_ui->draw_rect(x, y, width, height, 0.08f, 0.08f, 0.12f, 1.0f);

    // Header
    m_ui->draw_text(x + 10.0f, y + 4.0f, "Brushes (by time)", 0.9f, 0.9f, 0.9f, 1.0f);

    float listY = y + 24.0f;
    const float lineHeight = 18.0f;

    if (!m_brushes) return;

    for (int sortedIdx : m_sortedIndices) {
        const Brush& b = (*m_brushes)[sortedIdx];
        char buf[128];
        snprintf(buf, sizeof(buf), "[%d] %s %s (%.1f,%.1f,%.1f)",
            b.time,
            (b.type == BrushType::Add) ? "Add" : "Sub",
            (b.shape == ShapeType::Box) ? "Box" : "Wedge",
            b.center.x, b.center.y, b.center.z);

        bool selected = (sortedIdx == m_selectedIndex);
        float r = selected ? 1.0f : 0.7f;
        float g = selected ? 1.0f : 0.7f;
        float bl = selected ? 0.2f : 0.7f;

        // Highlight background for selected
        if (selected) {
            m_ui->draw_rect(x + 2.0f, listY - 2.0f, width - 4.0f, lineHeight, 0.2f, 0.3f, 0.5f, 0.8f);
        }
        m_ui->draw_text(x + 8.0f, listY, buf, r, g, bl, 1.0f);
        listY += lineHeight;
        if (listY > m_ui->get_height() - 20.0f) break;
    }
}

// --------------------------------------------------------------------------
// Right panel: inspector for selected brush
// --------------------------------------------------------------------------
void EditorUI::draw_inspector() {
    const float x = (float)m_ui->get_width() - RIGHT_PANEL_WIDTH;
    const float y = TOP_MENU_HEIGHT;
    const float width = RIGHT_PANEL_WIDTH;
    const float height = (float)m_ui->get_height() - y;

    // Panel background
    m_ui->draw_rect(x, y, width, height, 0.08f, 0.08f, 0.12f, 1.0f);

    if (m_selectedIndex < 0 || !m_brushes || m_selectedIndex >= (int)m_brushes->size()) {
        m_ui->draw_text(x + 10.0f, y + 10.0f, "No selection", 0.6f, 0.6f, 0.6f, 1.0f);
        return;
    }

    const Brush& b = (*m_brushes)[m_selectedIndex];
    float startY = y + 10.0f;
    float lineHeight = 20.0f;
    const float labelX = x + 10.0f;
    const float valueX = x + 80.0f;
    const float fieldWidth = 100.0f;

    m_ui->draw_text(labelX, startY, "Inspector", 1.0f, 1.0f, 1.0f, 1.0f);
    startY += lineHeight;

    auto draw_edit_field = [&](const char* label, float value, EditField field) {
        bool active = (m_editField == field);
        char buf[64];
        snprintf(buf, sizeof(buf), "%s:", label);
        m_ui->draw_text(labelX, startY, buf, 0.8f, 0.8f, 0.8f, 1.0f);

        // Draw the value as clickable
        char valStr[32];
        snprintf(valStr, sizeof(valStr), active ? "[%.3f]" : "%.3f", value);
        float r = active ? 1.0f : 0.9f;
        float g = active ? 0.9f : 0.9f;
        float bl = active ? 0.2f : 0.9f;
        m_ui->draw_text(valueX, startY, valStr, r, g, bl, 1.0f);

        // If active, show the edit buffer below
        if (active) {
            m_ui->draw_text(valueX, startY + 16.0f, m_editBuffer.c_str(), 1.0f, 1.0f, 0.0f, 1.0f);
        }
        startY += lineHeight;
    };

    draw_edit_field("Pos X", b.center.x, EditField::PosX);
    draw_edit_field("Pos Y", b.center.y, EditField::PosY);
    draw_edit_field("Pos Z", b.center.z, EditField::PosZ);
    draw_edit_field("Size X", b.size.x, EditField::SizeX);
    draw_edit_field("Size Y", b.size.y, EditField::SizeY);
    draw_edit_field("Size Z", b.size.z, EditField::SizeZ);

    // Buttons: Apply, Cancel, Delete
    float btnY = startY + 10.0f;
    m_ui->draw_text(valueX, btnY, "[Apply]", 0.0f, 1.0f, 0.0f, 1.0f);
    m_ui->draw_text(valueX + 70.0f, btnY, "[Cancel]", 1.0f, 0.0f, 0.0f, 1.0f);
    m_ui->draw_text(valueX + 150.0f, btnY, "[Delete]", 1.0f, 0.5f, 0.0f, 1.0f);
}

// --------------------------------------------------------------------------
// Hit test helper
// --------------------------------------------------------------------------
bool EditorUI::hit_test_rect(int x, int y, float rx, float ry, float rw, float rh) const {
    return (x >= rx && x <= rx + rw && y >= ry && y <= ry + rh);
}

// --------------------------------------------------------------------------
// Mouse click handling
// --------------------------------------------------------------------------
bool EditorUI::on_mouse_click(int x, int y) {
    if (!m_ui) return false;

    // ---- 1. Top menu ----
    const float menuY = 0.0f;
    const float menuH = TOP_MENU_HEIGHT;
    if (hit_test_rect(x, y, 0.0f, menuY, (float)m_ui->get_width(), menuH)) {
        // Check which menu item
        if (hit_test_rect(x, y, 10.0f, menuY + 6.0f, 50.0f, 20.0f)) {
            m_activeMenu = (m_activeMenu == Menu::File) ? Menu::None : Menu::File;
            return true;
        }
        if (hit_test_rect(x, y, 70.0f, menuY + 6.0f, 50.0f, 20.0f)) {
            m_activeMenu = (m_activeMenu == Menu::Edit) ? Menu::None : Menu::Edit;
            return true;
        }
        if (hit_test_rect(x, y, 130.0f, menuY + 6.0f, 50.0f, 20.0f)) {
            m_activeMenu = (m_activeMenu == Menu::Build) ? Menu::None : Menu::Build;
            // Build -> save level (CSG rebuild)
            if (m_activeMenu == Menu::Build) {
                m_editor->save_level();
                m_activeMenu = Menu::None;
            }
            return true;
        }
        // Click on menu bar but not on items: close menu
        m_activeMenu = Menu::None;
        return true;
    }

    // ---- 2. Left panel: brush list ----
    const float listX = 0.0f;
    const float listY = TOP_MENU_HEIGHT;
    const float listW = LEFT_PANEL_WIDTH;
    const float listH = (float)m_ui->get_height() - listY;
    if (hit_test_rect(x, y, listX, listY, listW, listH) && m_brushes) {
        float itemY = listY + 24.0f;
        const float lineHeight = 18.0f;
        for (int sortedIdx : m_sortedIndices) {
            if (hit_test_rect(x, y, listX + 2.0f, itemY - 2.0f, listW - 4.0f, lineHeight)) {
                // Select this brush
                m_editor->select_brush(sortedIdx);
                return true;
            }
            itemY += lineHeight;
        }
        return true; // consume click in panel even if no hit
    }

    // ---- 3. Right panel: inspector ----
    const float inspX = (float)m_ui->get_width() - RIGHT_PANEL_WIDTH;
    const float inspY = TOP_MENU_HEIGHT;
    const float inspW = RIGHT_PANEL_WIDTH;
    const float inspH = (float)m_ui->get_height() - inspY;
    if (hit_test_rect(x, y, inspX, inspY, inspW, inspH)) {
        if (m_selectedIndex < 0) return true;

        // Check edit fields
        float fieldY = inspY + 10.0f + 20.0f; // after "Inspector" header
        const float labelX = inspX + 10.0f;
        const float valueX = inspX + 80.0f;
        const float fieldH = 20.0f;

        // Fields in order: PosX, PosY, PosZ, SizeX, SizeY, SizeZ
        EditField fields[] = {
            EditField::PosX, EditField::PosY, EditField::PosZ,
            EditField::SizeX, EditField::SizeY, EditField::SizeZ
        };
        for (int i = 0; i < 6; ++i) {
            if (hit_test_rect(x, y, valueX, fieldY, 100.0f, fieldH)) {
                m_editField = fields[i];
                // Initialize edit buffer with current value
                const Brush& b = (*m_brushes)[m_selectedIndex];
                float val = 0.0f;
                switch (m_editField) {
                    case EditField::PosX: val = b.center.x; break;
                    case EditField::PosY: val = b.center.y; break;
                    case EditField::PosZ: val = b.center.z; break;
                    case EditField::SizeX: val = b.size.x; break;
                    case EditField::SizeY: val = b.size.y; break;
                    case EditField::SizeZ: val = b.size.z; break;
                    default: break;
                }
                m_editBuffer = std::to_string(val);
                return true;
            }
            fieldY += 20.0f;
        }

        // Buttons: Apply, Cancel, Delete
        float btnY = fieldY + 10.0f;
        if (hit_test_rect(x, y, valueX, btnY, 60.0f, 20.0f)) {
            // Apply
            if (m_editField != EditField::None) {
                float val = std::stof(m_editBuffer);
                m_editor->apply_brush_edit((int)m_editField, val);
                m_editField = EditField::None;
                m_editBuffer.clear();
            }
            return true;
        }
        if (hit_test_rect(x, y, valueX + 70.0f, btnY, 60.0f, 20.0f)) {
            // Cancel
            m_editField = EditField::None;
            m_editBuffer.clear();
            return true;
        }
        if (hit_test_rect(x, y, valueX + 150.0f, btnY, 60.0f, 20.0f)) {
            // Delete
            m_editor->delete_selected();
            return true;
        }
        return true; // consume all clicks in inspector panel
    }

    // Not consumed
    return false;
}

// --------------------------------------------------------------------------
// Keyboard input (for editing)
// --------------------------------------------------------------------------
bool EditorUI::on_key_down(int key) {
    // We don't handle key down events for editing; we use WM_CHAR for text input.
    return false;
}

bool EditorUI::on_text_input(char c) {
    if (m_editField == EditField::None) return false;

    if (c == '\r' || c == '\n') {
        // Apply
        if (m_editField != EditField::None) {
            float val = std::stof(m_editBuffer);
            m_editor->apply_brush_edit((int)m_editField, val);
            m_editField = EditField::None;
            m_editBuffer.clear();
        }
        return true;
    }
    if (c == 0x1B) { // Escape
        m_editField = EditField::None;
        m_editBuffer.clear();
        return true;
    }
    if (c == 0x08) { // Backspace
        if (!m_editBuffer.empty()) m_editBuffer.pop_back();
        return true;
    }
    if ((c >= '0' && c <= '9') || c == '-' || c == '.') {
        m_editBuffer.push_back(c);
        return true;
    }
    return false;
}