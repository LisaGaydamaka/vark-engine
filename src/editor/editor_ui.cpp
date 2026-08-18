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

// Filter callback: allow only digits
static int TimeInputFilter(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) {
        if (data->EventChar >= '0' && data->EventChar <= '9') {
            return 0; // accept
        }
        return 1; // reject
    }
    return 0;
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
// src/editor/editor_ui.cpp – BuildLeftPanel with custom drag‑and‑drop

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

    // Static state for inline editing (Time / Name)
    static int editingIndex = -1;
    static int editingColumn = -1;  // 0 = Time, 1 = Name
    static char editingBuffer[128] = "";

    // ---- Drag‑and‑drop state ----
    static int dragSrc = -1;
    static bool isDragging = false;
    static bool dragCandidate = false;
    static int dragCandidateIdx = -1;
    static ImVec2 dragStartMouse;
    static int dropTarget = -1;

    struct ItemRect { int sortedIdx; float topY; float bottomY; };
    std::vector<ItemRect> itemRects;
    itemRects.reserve(brushes.size());

    // Sort brushes by time (ascending)
    std::vector<int> sortedIndices;
    sortedIndices.resize(brushes.size());
    for (int i = 0; i < (int)brushes.size(); ++i) sortedIndices[i] = i;
    std::sort(sortedIndices.begin(), sortedIndices.end(),
        [&](int a, int b) { return brushes[a].time < brushes[b].time; });

    ImGui::BeginChild("BrushList", ImVec2(0, 0), true);

    // ---- Table setup ----
    if (ImGui::BeginTable("BrushTable", 2,
        ImGuiTableFlags_Resizable | ImGuiTableFlags_NoBordersInBody))
    {
        // Column 0: Time (fixed width ~40px)
        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 40.0f);
        // Column 1: Brush name (stretches)
        ImGui::TableSetupColumn("Brush", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        // ---- Render each brush row ----
        for (int idx : sortedIndices) {
            const auto& b = brushes[idx];
            bool isSelected = (idx == selected);
            bool isEditing = (idx == editingIndex);

            ImGui::TableNextRow();
            ImVec2 rowMin(FLT_MAX, FLT_MAX);
            ImVec2 rowMax(-FLT_MAX, -FLT_MAX);

            // ---- Column 0: Time ----
            ImGui::TableSetColumnIndex(0);
            ImGui::PushID(idx);
            if (isEditing && editingColumn == 0) {
                bool commit = false;
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::SetKeyboardFocusHere(0);
                if (ImGui::InputText("##edit_time", editingBuffer, sizeof(editingBuffer),
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll |
                    ImGuiInputTextFlags_CallbackCharFilter,
                    TimeInputFilter))
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
                ImVec2 min = ImGui::GetItemRectMin();
                ImVec2 max = ImGui::GetItemRectMax();
                rowMin.x = std::min(rowMin.x, min.x); rowMin.y = std::min(rowMin.y, min.y);
                rowMax.x = std::max(rowMax.x, max.x); rowMax.y = std::max(rowMax.y, max.y);
            } else {
                char timeLabel[32];
                snprintf(timeLabel, sizeof(timeLabel), "%d", b.time);
                // Use selectable with no background to look like a text cell
                bool sel = ImGui::Selectable(timeLabel, isSelected);
                if (sel && !isDragging) {
                    editor.select_brush(idx);
                }
                if (ImGui::IsItemClicked(0) && !isDragging && !ImGui::GetIO().MouseDoubleClicked[0]) {
                    dragCandidate = true;
                    dragCandidateIdx = idx;
                    dragStartMouse = ImGui::GetIO().MousePos;
                }
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    editingIndex = idx;
                    editingColumn = 0;
                    snprintf(editingBuffer, sizeof(editingBuffer), "%d", b.time);
                }
                ImVec2 min = ImGui::GetItemRectMin();
                ImVec2 max = ImGui::GetItemRectMax();
                rowMin.x = std::min(rowMin.x, min.x); rowMin.y = std::min(rowMin.y, min.y);
                rowMax.x = std::max(rowMax.x, max.x); rowMax.y = std::max(rowMax.y, max.y);
            }
            ImGui::PopID();

            // ---- Column 1: Brush name ----
            ImGui::TableSetColumnIndex(1);
            ImGui::PushID(idx + 1000); // separate ID for second column
            if (isEditing && editingColumn == 1) {
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
                ImVec2 min = ImGui::GetItemRectMin();
                ImVec2 max = ImGui::GetItemRectMax();
                rowMin.x = std::min(rowMin.x, min.x); rowMin.y = std::min(rowMin.y, min.y);
                rowMax.x = std::max(rowMax.x, max.x); rowMax.y = std::max(rowMax.y, max.y);
            } else {
                bool sel = ImGui::Selectable(b.name.c_str(), isSelected);
                if (sel && !isDragging) {
                    editor.select_brush(idx);
                }
                if (ImGui::IsItemClicked(0) && !isDragging && !ImGui::GetIO().MouseDoubleClicked[0]) {
                    dragCandidate = true;
                    dragCandidateIdx = idx;
                    dragStartMouse = ImGui::GetIO().MousePos;
                }
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    editingIndex = idx;
                    editingColumn = 1;
                    strncpy_s(editingBuffer, sizeof(editingBuffer), b.name.c_str(), _TRUNCATE);
                }
                ImVec2 min = ImGui::GetItemRectMin();
                ImVec2 max = ImGui::GetItemRectMax();
                rowMin.x = std::min(rowMin.x, min.x); rowMin.y = std::min(rowMin.y, min.y);
                rowMax.x = std::max(rowMax.x, max.x); rowMax.y = std::max(rowMax.y, max.y);
            }
            ImGui::PopID();

            // Store row vertical range (screen coordinates)
            if (rowMin.x < FLT_MAX && rowMax.x > -FLT_MAX) {
                itemRects.push_back({ idx, rowMin.y, rowMax.y });
            }
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();

    // ---- Drag‑and‑drop logic (unchanged from previous version) ----
    if (dragCandidate && !isDragging) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        float dx = mouse.x - dragStartMouse.x;
        float dy = mouse.y - dragStartMouse.y;
        if (dx*dx + dy*dy > 16.0f) {
            isDragging = true;
            dragSrc = dragCandidateIdx;
            dragCandidate = false;
        }
    }

    if (isDragging) {
        float mouseY = ImGui::GetIO().MousePos.y;
        int N = (int)itemRects.size();
        dropTarget = N;
        for (int i = 0; i < N; ++i) {
            const auto& r = itemRects[i];
            if (mouseY >= r.topY && mouseY < r.bottomY) {
                float mid = (r.topY + r.bottomY) * 0.5f;
                dropTarget = (mouseY < mid) ? i : i + 1;
                break;
            }
            if (mouseY < r.topY) {
                dropTarget = i;
                break;
            }
        }
        if (dropTarget < 0) dropTarget = 0;
        if (dropTarget > N) dropTarget = N;

        float lineY;
        if (dropTarget < N) {
            lineY = itemRects[dropTarget].topY;
        } else {
            lineY = itemRects.empty() ? ImGui::GetCursorScreenPos().y : itemRects.back().bottomY;
        }
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(ImGui::GetWindowPos().x, lineY),
            ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), lineY),
            IM_COL32(255, 255, 0, 255), 2.0f
        );
    }

    if (ImGui::IsMouseReleased(0)) {
        if (isDragging && dragSrc != -1 && dropTarget != -1) {
            int N = (int)itemRects.size();
            int srcSorted = dragSrc;
            int dst = dropTarget;

            LOG_INFO("=== DRAG RELEASE ===");
            LOG_INFO("srcSorted = %d, dst = %d, N = %d", srcSorted, dst, N);

            // Log current order
            LOG_INFO("Current order (time -> brush):");
            for (int i = 0; i < N; ++i) {
                int idx = sortedIndices[i];
                const auto& b = brushes[idx];
                LOG_INFO("  sorted[%d] = brush[%d], time=%d, name='%s'", i, idx, b.time, b.name.c_str());
            }

            int targetIndex = (dst > srcSorted) ? (dst - 1) : dst;
            if (targetIndex < 0) targetIndex = 0;
            if (targetIndex >= N) targetIndex = N - 1;

            int originalIdx = sortedIndices[srcSorted];
            int currentTime = brushes[originalIdx].time;

            LOG_INFO("originalIdx = %d, currentTime = %d, targetIndex = %d", originalIdx, currentTime, targetIndex);

            if (targetIndex != currentTime) {
                editor.set_brush_time(originalIdx, targetIndex);
                LOG_INFO("Reordered brush %d from time %d to %d", originalIdx, currentTime, targetIndex);

                // Log new order after reorder
                std::vector<int> newSorted;
                newSorted.resize(brushes.size());
                for (int i = 0; i < (int)brushes.size(); ++i) newSorted[i] = i;
                std::sort(newSorted.begin(), newSorted.end(),
                    [&](int a, int b) { return brushes[a].time < brushes[b].time; });
                LOG_INFO("New order after reorder:");
                for (int i = 0; i < (int)newSorted.size(); ++i) {
                    int idx = newSorted[i];
                    const auto& b = brushes[idx];
                    LOG_INFO("  sorted[%d] = brush[%d], time=%d, name='%s'", i, idx, b.time, b.name.c_str());
                }
            } else {
                LOG_INFO("No reorder needed (target time equals current)");
            }
        }

        isDragging = false;
        dragCandidate = false;
        dragSrc = -1;
        dragCandidateIdx = -1;
        dropTarget = -1;
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