// src/editor/editor_ui.cpp
#include "editor_ui.h"
#include "editor/editor.h"
#include "world/level.h"
#include "core/logger.h"
#include "imgui.h"
#include <cstdio>
#include <algorithm>
#include <vector>
#include <cstring>
#include <cstdlib>

static void BuildMenuBar(Editor& editor);
static void BuildLeftPanel(Editor& editor);
static void BuildRightPanel(Editor& editor);

void BuildEditorUI(Editor& editor) {
    BuildMenuBar(editor);
    BuildLeftPanel(editor);
    BuildRightPanel(editor);
}

// ------------------------------------------------------------------
// Menu Bar
// ------------------------------------------------------------------
static void BuildMenuBar(Editor& editor) {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                editor.save_level();
            }
            if (ImGui::MenuItem("Load", "Ctrl+O")) {
                LOG_INFO("Load menu clicked (not implemented)");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                PostQuitMessage(0);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Delete Selected", "Del")) {
                editor.delete_selected();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Build")) {
            if (ImGui::MenuItem("Compile Level")) {
                editor.save_level();
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

// ------------------------------------------------------------------
// Left Panel – Brush List with editable Time and Name
// ------------------------------------------------------------------
static void BuildLeftPanel(Editor& editor) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 workPos = viewport->WorkPos;
    ImVec2 workSize = viewport->WorkSize;

    static float panelWidth = 280.0f;

    ImGui::SetNextWindowPos(workPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelWidth, workSize.y), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 4.0f));
    ImGui::Begin("Brushes", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings);

    // ---- Buttons ----
    if (ImGui::Button("Add Box")) {
        editor.add_brush(BrushType::Add, ShapeType::Box);
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Wedge")) {
        editor.add_brush(BrushType::Add, ShapeType::Wedge);
    }
    ImGui::SameLine();
    if (ImGui::Button("Sub Box")) {
        editor.add_brush(BrushType::Sub, ShapeType::Box);
    }
    ImGui::SameLine();
    if (ImGui::Button("Sub Wedge")) {
        editor.add_brush(BrushType::Sub, ShapeType::Wedge);
    }

    ImGui::Separator();

    // ---- Brush list ----
    const auto& brushes = editor.get_brushes();
    int selected = editor.get_selected_index();

    // Static state for inline editing
    static int editingIndex = -1;
    static int editingColumn = -1;  // 0 = Time, 1 = Name
    static char editingBuffer[128] = "";

    ImGui::BeginChild("BrushList", ImVec2(0, 0), true);

    if (ImGui::BeginTable("BrushTable", 2,
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_NoBordersInBody |
            ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 40.0f);
        ImGui::TableSetupColumn("Brush", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        // Sort brushes by time (ascending)
        std::vector<int> sortedIndices;
        sortedIndices.resize(brushes.size());
        for (int i = 0; i < (int)brushes.size(); ++i) sortedIndices[i] = i;
        std::sort(sortedIndices.begin(), sortedIndices.end(),
            [&](int a, int b) { return brushes[a].time < brushes[b].time; });

        for (int idx : sortedIndices) {
            const auto& b = brushes[idx];
            bool isSelected = (idx == selected);
            bool isEditing = (idx == editingIndex);

            ImGui::TableNextRow();

            // ---- Row unique ID ----
            ImGui::PushID(idx);

            // ---- Time column ----
            ImGui::TableSetColumnIndex(0);
            if (isEditing && editingColumn == 0) {
                // Editing Time – zero padding, full width, auto select all
                bool commit = false;
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::SetKeyboardFocusHere(0);
                if (ImGui::InputText("##edit_time", editingBuffer, sizeof(editingBuffer),
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
                {
                    commit = true;
                }
                ImGui::PopStyleVar();
                if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(0)) {
                    commit = true;
                }
                if (commit) {
                    int newTime = atoi(editingBuffer);
                    editor.set_brush_time(idx, newTime);
                    editingIndex = -1;
                }
            } else {
                // Normal Time cell: clickable to select, double-click to edit
                char timeLabel[32];
                snprintf(timeLabel, sizeof(timeLabel), "%d", b.time);
                ImGui::PushID("time");
                if (ImGui::Selectable(timeLabel, isSelected, ImGuiSelectableFlags_None)) {
                    editor.select_brush(idx);
                }
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    editingIndex = idx;
                    editingColumn = 0;
                    snprintf(editingBuffer, sizeof(editingBuffer), "%d", b.time);
                }
                ImGui::PopID();
            }

            // ---- Name column ----
            ImGui::TableSetColumnIndex(1);
            if (isEditing && editingColumn == 1) {
                // Editing Name – zero padding, full width, auto select all
                bool commit = false;
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::SetKeyboardFocusHere(0);
                if (ImGui::InputText("##edit_name", editingBuffer, sizeof(editingBuffer),
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
                {
                    commit = true;
                }
                ImGui::PopStyleVar();
                if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(0)) {
                    commit = true;
                }
                if (commit) {
                    editor.set_brush_name(idx, editingBuffer);
                    editingIndex = -1;
                }
            } else {
                // Normal Name cell: clickable to select, double-click to edit
                ImGui::PushID("name");
                if (ImGui::Selectable(b.name.c_str(), isSelected, ImGuiSelectableFlags_None)) {
                    editor.select_brush(idx);
                }
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    editingIndex = idx;
                    editingColumn = 1;
                    strncpy_s(editingBuffer, sizeof(editingBuffer), b.name.c_str(), _TRUNCATE);
                }
                ImGui::PopID();
            }

            // ---- End row ID ----
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::EndChild();
    ImGui::End();
    ImGui::PopStyleVar();
}

// ------------------------------------------------------------------
// Right Panel – Inspector
// ------------------------------------------------------------------
static void BuildRightPanel(Editor& editor) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 workPos = viewport->WorkPos;
    ImVec2 workSize = viewport->WorkSize;

    static float panelWidth = 320.0f;

    ImGui::SetNextWindowPos(ImVec2(workPos.x + workSize.x - panelWidth, workPos.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelWidth, workSize.y), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 4.0f));
    ImGui::Begin("Inspector", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings);

    int sel = editor.get_selected_index();
    if (sel < 0 || sel >= (int)editor.get_brushes().size()) {
        ImGui::Text("No brush selected");
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    const Brush& b = editor.get_brushes()[sel];

    ImGui::Text("Brush #%d", sel);
    ImGui::Separator();

    float pos[3] = { b.center.x, b.center.y, b.center.z };
    if (ImGui::DragFloat3("Position", pos, 0.1f)) {
        editor.apply_brush_edit((int)EditField::PosX, pos[0]);
        editor.apply_brush_edit((int)EditField::PosY, pos[1]);
        editor.apply_brush_edit((int)EditField::PosZ, pos[2]);
    }

    float size[3] = { b.size.x, b.size.y, b.size.z };
    if (ImGui::DragFloat3("Size", size, 0.1f, 0.1f, 100.0f)) {
        editor.apply_brush_edit((int)EditField::SizeX, size[0]);
        editor.apply_brush_edit((int)EditField::SizeY, size[1]);
        editor.apply_brush_edit((int)EditField::SizeZ, size[2]);
    }

    ImGui::Text("Type: %s", (b.type == BrushType::Add) ? "Add" : "Sub");
    ImGui::Text("Shape: %s", (b.shape == ShapeType::Box) ? "Box" : "Wedge");

    ImGui::Separator();
    ImGui::Text("Face Textures (first face only)");
    const FaceTexture& ft = b.faces[0];
    ImGui::Text("Texture: %s", ft.texturePath.c_str());
    float off[2] = { ft.offset.x, ft.offset.y };
    if (ImGui::DragFloat2("Offset", off, 0.1f)) {
        LOG_INFO("Face offset change not implemented");
    }
    float sc[2] = { ft.scale.x, ft.scale.y };
    if (ImGui::DragFloat2("Scale", sc, 0.1f, 0.1f, 100.0f)) {
        LOG_INFO("Face scale change not implemented");
    }

    ImGui::End();
    ImGui::PopStyleVar();
}