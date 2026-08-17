#include "editor_ui.h"
#include "editor/editor.h"
#include "world/level.h"
#include "core/logger.h"
#include "imgui.h"
#include <cstdio>

static void BuildMenuBar(Editor& editor);
static void BuildLeftPanel(Editor& editor);
static void BuildRightPanel(Editor& editor);

void BuildEditorUI(Editor& editor) {
    BuildMenuBar(editor);
    BuildLeftPanel(editor);
    BuildRightPanel(editor);
}

// ------------------------------------------------------------------
// Menu Bar (unchanged)
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
// Left Panel – Brush List with Table, Drag‑Drop, Rename
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

    // ---- Add buttons ----
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

    const auto& brushes = editor.get_brushes();
    int selected = editor.get_selected_index();

    // ---- Table ----
    if (ImGui::BeginTable("BrushTable", 2,
            ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 40.0f);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (int i = 0; i < (int)brushes.size(); ++i) {
            ImGui::PushID(i);   // <--- UNIQUE ID PER ROW
            const auto& b = brushes[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", b.time);
            ImGui::TableSetColumnIndex(1);

            // Selectable row
            if (ImGui::Selectable(b.displayName.c_str(), (selected == i),
                    ImGuiSelectableFlags_SpanAllColumns)) {
                editor.select_brush(i);
            }

            // ---- Drag source ----
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("BRUSH_ORDER", &i, sizeof(int));
                ImGui::Text("Move %s", b.displayName.c_str());
                ImGui::EndDragDropSource();
            }

            // ---- Drop target ----
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BRUSH_ORDER")) {
                    int srcIndex = *(const int*)payload->Data;
                    if (srcIndex != i) {
                        auto& mutableBrushes = editor.get_brushes_mutable();
                        std::swap(mutableBrushes[srcIndex], mutableBrushes[i]);
                        int oldSel = editor.get_selected_index();
                        if (oldSel == srcIndex) editor.select_brush(i);
                        else if (oldSel == i) editor.select_brush(srcIndex);
                        editor.refresh_times();
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // ---- Rename popup on right-click ----
            if (ImGui::BeginPopupContextItem()) {
                static char nameBuffer[128];
                strcpy_s(nameBuffer, b.displayName.c_str());
                if (ImGui::InputText("Rename", nameBuffer, sizeof(nameBuffer),
                        ImGuiInputTextFlags_EnterReturnsTrue)) {
                    auto& mutableBrushes = editor.get_brushes_mutable();
                    mutableBrushes[i].displayName = nameBuffer;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }
        ImGui::EndTable();
    }

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
    ImGui::Text("Time: %d", b.time);
    ImGui::Text("Name: %s", b.displayName.c_str());

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