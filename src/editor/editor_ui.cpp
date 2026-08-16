// src/editor/editor_ui.cpp
#include "editor_ui.h"
#include "editor.h"
#include <cstdio>
#include <cstring>

EditorUI::EditorUI(Editor* editor, UIRenderer* ui)
    : m_editor(editor), m_ui(ui) {
}

void EditorUI::set_brushes(const std::vector<Brush>& brushes) {
    m_brushes = &brushes;
}

void EditorUI::set_selected(int index) {
    m_selectedIndex = index;
}

void EditorUI::render() {
    if (!m_ui) return;
    draw_brush_list();
    draw_properties();
}

void EditorUI::draw_brush_list() {
    if (!m_brushes) return;

    float y = 50.0f;
    m_ui->draw_text(10.0f, y, "=== Brush List ===", 1.0f, 1.0f, 1.0f, 1.0f);
    y += 20.0f;

    for (size_t i = 0; i < m_brushes->size(); ++i) {
        const auto& b = (*m_brushes)[i];
        char buf[256];
        snprintf(buf, sizeof(buf), "[%zu] %s %s (%.1f,%.1f,%.1f)",
                 i,
                 (b.type == BrushType::Add) ? "Add" : "Sub",
                 (b.shape == ShapeType::Box) ? "Box" : "Wedge",
                 b.center.x, b.center.y, b.center.z);
        bool sel = (i == (size_t)m_selectedIndex);
        float r = sel ? 1.0f : 0.7f;
        float g = sel ? 1.0f : 0.7f;
        float bl = sel ? 0.2f : 0.7f;
        m_ui->draw_text(20.0f, y, buf, r, g, bl, 1.0f);
        y += 16.0f;
        if (y > (float)m_ui->get_height() - 40.0f) break;
    }
}

void EditorUI::draw_properties() {
    if (m_selectedIndex < 0 || !m_brushes || m_selectedIndex >= (int)m_brushes->size()) return;

    const auto& b = (*m_brushes)[m_selectedIndex];
    float startX = 200.0f, startY = 50.0f;
    float lineHeight = 20.0f;

    m_ui->draw_text(startX, startY, "Properties:", 1.0f, 1.0f, 1.0f, 1.0f);
    startY += lineHeight;

    draw_edit_field("Pos X", b.center.x, EditField::PosX, startY);
    draw_edit_field("Pos Y", b.center.y, EditField::PosY, startY);
    draw_edit_field("Pos Z", b.center.z, EditField::PosZ, startY);
    draw_edit_field("Size X", b.size.x, EditField::SizeX, startY);
    draw_edit_field("Size Y", b.size.y, EditField::SizeY, startY);
    draw_edit_field("Size Z", b.size.z, EditField::SizeZ, startY);

    // Apply/Cancel buttons (just visual for now; click handled in on_mouse_click)
    float btnY = startY + 10.0f;
    m_ui->draw_text(startX, btnY, "[Apply]", 0.0f, 1.0f, 0.0f, 1.0f);
    m_ui->draw_text(startX + 70.0f, btnY, "[Cancel]", 1.0f, 0.0f, 0.0f, 1.0f);
}

void EditorUI::draw_edit_field(const char* label, float value, EditField field, float& y) {
    char buf[64];
    bool active = (m_editField == field);
    snprintf(buf, sizeof(buf), "%s: %s%.3f", label, active ? "[" : "", value);
    float r = active ? 1.0f : 0.8f;
    float g = active ? 0.8f : 0.8f;
    float bl = active ? 0.2f : 0.8f;
    m_ui->draw_text(200.0f, y, buf, r, g, bl, 1.0f);
    if (active) {
        m_ui->draw_text(220.0f, y + 16.0f, m_editBuffer.c_str(), 1.0f, 1.0f, 0.0f, 1.0f);
    }
    y += 20.0f;
}

bool EditorUI::on_mouse_click(int x, int y) {
    if (!m_ui) return false;

    // Check if click is on any edit field label
    // We'll check a rough area (x between 200 and 350, y based on the field positions)
    // For simplicity, we'll just detect if the user clicked in the properties area.
    // A full implementation would store the rect of each field and test.

    // We'll use a crude approach: if y is between 70 and 190 (approx 6 fields * 20), and x between 200 and 350.
    // In a real implementation, we'd compute exact positions.
    float fy = (float)y;
    if (x >= 200 && x <= 350 && fy >= 70 && fy <= 190) {
        int fieldIndex = (int)((fy - 70) / 20);
        if (fieldIndex >= 0 && fieldIndex <= 5) {
            EditField fields[] = {
                EditField::PosX, EditField::PosY, EditField::PosZ,
                EditField::SizeX, EditField::SizeY, EditField::SizeZ
            };
            if (fieldIndex < 6) {
                m_editField = fields[fieldIndex];
                // Initialize edit buffer with current value
                if (m_brushes && m_selectedIndex >= 0 && m_selectedIndex < (int)m_brushes->size()) {
                    const auto& b = (*m_brushes)[m_selectedIndex];
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
                }
                return true;
            }
        }
    }

    // Check Apply/Cancel buttons
    if (x >= 200 && x <= 270 && fy >= 200 && fy <= 220) {
        // Apply
        if (m_editField != EditField::None) {
            float val = std::stof(m_editBuffer);
            m_editor->apply_brush_edit((int)m_editField, val);
            m_editField = EditField::None;
            m_editBuffer.clear();
        }
        return true;
    }
    if (x >= 270 && x <= 340 && fy >= 200 && fy <= 220) {
        // Cancel
        m_editField = EditField::None;
        m_editBuffer.clear();
        return true;
    }

    return false;
}

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

bool EditorUI::is_point_in_rect(int x, int y, float rx, float ry, float rw, float rh) const {
    return (x >= rx && x <= rx + rw && y >= ry && y <= ry + rh);
}