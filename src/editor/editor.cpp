// src/editor/editor.cpp
#include "editor.h"
#include "core/logger.h"
#include "core/geometry.h"
#include "compiler/csg_solver.h"
#include "world/vmis_io.h"
#include <d3d11.h>
#include <algorithm>
#include <unordered_map>
#include <string>
#include <vector>
#include <cstring>

Editor::Editor() {}
Editor::~Editor() { shutdown(); }

bool Editor::initialize(Renderer* renderer, Level* level, UIRenderer* ui) {
    m_renderer = renderer;
    m_level = level;
    m_ui = ui;

    if (!m_renderer || !m_level || !m_ui) return false;

    ID3D11Device* device = (ID3D11Device*)m_renderer->get_device();
    if (!device) {
        LOG_ERROR("Editor: no D3D11 device");
        return false;
    }

    // Create dynamic buffer for wireframes
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = 1024 * 1024; // 1 MB initial
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(device->CreateBuffer(&bd, nullptr, m_wireframeBuffer.GetAddressOf()))) {
        LOG_WARN("Editor: could not create wireframe buffer");
    }

    m_initialized = true;
    LOG_INFO("Editor initialized.");
    return true;
}

void Editor::shutdown() {
    m_wireframeBuffer.Reset();
    m_initialized = false;
}

void Editor::sync_brushes() {
    m_brushes = m_level->get_brushes();
    if (m_selectedIndex >= (int)m_brushes.size()) {
        m_selectedIndex = -1;
    }
}

void Editor::update(float dt) {
    // EditorCamera updates itself via orbit/pan/zoom calls.
    (void)dt;
}

void Editor::set_keybinds(const EditorKeybindSettings& keybinds) {
    m_keybinds = keybinds;
}

// ---------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------
void Editor::render() {
    if (!m_initialized) return;

    // ---- Brush wireframes ----
    rebuild_wireframe_buffer();
    if (m_wireframeBuffer && m_wireframeVertexCount > 0) {
        m_renderer->draw_lines(m_wireframeBuffer.Get(), m_wireframeVertexCount);
    }

    // ---- 2D UI overlay ----
    float y = 50.0f;
    m_ui->draw_text(10.0f, y, "=== Brush List ===", 1.0f, 1.0f, 1.0f, 1.0f);
    y += 20.0f;

    for (size_t i = 0; i < m_brushes.size(); ++i) {
        const auto& b = m_brushes[i];
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

    // ---- Properties panel ----
    if (m_selectedIndex >= 0 && m_selectedIndex < (int)m_brushes.size()) {
        const auto& b = m_brushes[m_selectedIndex];
        char info[512];
        snprintf(info, sizeof(info),
                 "Selected #%d\n"
                 "Type: %s\nShape: %s\n"
                 "Pos: (%.1f, %.1f, %.1f)\n"
                 "Size: (%.1f, %.1f, %.1f)",
                 m_selectedIndex,
                 (b.type == BrushType::Add) ? "Add" : "Sub",
                 (b.shape == ShapeType::Box) ? "Box" : "Wedge",
                 b.center.x, b.center.y, b.center.z,
                 b.size.x, b.size.y, b.size.z);
        m_ui->draw_text(200.0f, 50.0f, info, 1.0f, 1.0f, 1.0f, 1.0f);
    }
}

// ---------------------------------------------------------------------
// Wireframe buffer rebuild
// ---------------------------------------------------------------------
void Editor::rebuild_wireframe_buffer() {
    if (!m_wireframeBuffer) return;

    std::vector<Vertex> lineVerts;
    lineVerts.reserve(m_brushes.size() * 48); // rough estimate

    for (size_t i = 0; i < m_brushes.size(); ++i) {
        const auto& b = m_brushes[i];
        Vec3 color = (i == (size_t)m_selectedIndex) ? Vec3{1.0f, 1.0f, 0.0f} : Vec3{0.0f, 1.0f, 0.0f};
        if (b.shape == ShapeType::Box) {
            generate_box_wireframe(b.center, b.size, color, lineVerts);
        } else {
            generate_wedge_wireframe(b.center, b.size, color, lineVerts);
        }
    }

    if (lineVerts.empty()) {
        m_wireframeVertexCount = 0;
        return;
    }

    ID3D11DeviceContext* ctx = (ID3D11DeviceContext*)m_renderer->get_context();
    if (!ctx) return;

    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = ctx->Map(m_wireframeBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) return;
    size_t size = lineVerts.size() * sizeof(Vertex);
    if (size > 0) {
        memcpy(mapped.pData, lineVerts.data(), size);
    }
    ctx->Unmap(m_wireframeBuffer.Get(), 0);
    m_wireframeVertexCount = (int)lineVerts.size();
}

// ---------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------
void Editor::save_level() {
    if (m_brushes.empty()) {
        LOG_WARN("No brushes to save.");
        return;
    }

    // Build material table
    std::vector<VMBMaterial> materials;
    std::unordered_map<std::string, int> texToIndex;
    std::vector<int> materialIndices;
    materialIndices.reserve(m_brushes.size());

    for (const auto& brush : m_brushes) {
        const FaceTexture& ft = brush.faces[0];
        std::string key = ft.texturePath + "|" + std::to_string(ft.scale.x) + "|" + std::to_string(ft.scale.y);
        auto it = texToIndex.find(key);
        int matIdx;
        if (it == texToIndex.end()) {
            VMBMaterial mat;
            strcpy_s(mat.textureName, sizeof(mat.textureName), ft.texturePath.c_str());
            mat.uScale = ft.scale.x;
            mat.vScale = ft.scale.y;
            mat.rotation = ft.rotation;
            mat.offsetX = ft.offset.x;
            mat.offsetY = ft.offset.y;
            mat.worldLocked = ft.worldLocked ? 1 : 0;
            matIdx = (int)materials.size();
            materials.push_back(mat);
            texToIndex[key] = matIdx;
        } else {
            matIdx = it->second;
        }
        materialIndices.push_back(matIdx);
    }

    // Run CSG
    std::vector<CSGPoly> polys = compile_csg(m_brushes, materialIndices, materials);
    LOG_INFO("CSG produced %zu polygons", polys.size());

    // Convert to Vertex + indices
    std::vector<Vertex> bakedVerts;
    std::vector<uint32_t> bakedIndices;
    bakedVerts.reserve(polys.size() * 3);
    bakedIndices.reserve(polys.size() * 3);

    for (const auto& poly : polys) {
        size_t n = poly.verts.size();
        if (n < 3) continue;
        int baseIdx = (int)bakedVerts.size();
        for (size_t i = 0; i < n; ++i) {
            Vertex v;
            v.x = poly.verts[i].pos.x;
            v.y = poly.verts[i].pos.y;
            v.z = poly.verts[i].pos.z;
            v.u = poly.verts[i].uv.x;
            v.v = poly.verts[i].uv.y;
            v.r = v.g = v.b = 1.0f;
            bakedVerts.push_back(v);
        }
        for (size_t i = 1; i < n - 1; ++i) {
            bakedIndices.push_back(baseIdx);
            bakedIndices.push_back(baseIdx + (int)i);
            bakedIndices.push_back(baseIdx + (int)i + 1);
        }
    }

    // Convert materials to VMIS format
    std::vector<VMISMaterial> vmisMats;
    vmisMats.reserve(materials.size());
    for (const auto& mb : materials) {
        VMISMaterial vm;
        strcpy_s(vm.texturePath, sizeof(vm.texturePath), mb.textureName);
        vm.scaleX = mb.uScale;
        vm.scaleY = mb.vScale;
        vm.offsetX = mb.offsetX;
        vm.offsetY = mb.offsetY;
        vm.rotation = mb.rotation;
        vm.worldLocked = mb.worldLocked;
        vmisMats.push_back(vm);
    }

    // Physics triangles
    std::vector<Triangle> physTris;
    physTris.reserve(bakedIndices.size() / 3);
    for (size_t i = 0; i < bakedIndices.size(); i += 3) {
        Triangle tri;
        tri.v0 = { bakedVerts[bakedIndices[i]].x,   bakedVerts[bakedIndices[i]].y,   bakedVerts[bakedIndices[i]].z };
        tri.v1 = { bakedVerts[bakedIndices[i+1]].x, bakedVerts[bakedIndices[i+1]].y, bakedVerts[bakedIndices[i+1]].z };
        tri.v2 = { bakedVerts[bakedIndices[i+2]].x, bakedVerts[bakedIndices[i+2]].y, bakedVerts[bakedIndices[i+2]].z };
        physTris.push_back(tri);
    }

    const char* levelPath = m_level->get_level_path();
    if (!levelPath || !levelPath[0]) {
        LOG_ERROR("No level path set; cannot save.");
        return;
    }
    if (!write_vmis(levelPath, m_brushes, bakedVerts, bakedIndices, vmisMats, physTris, {})) {
        LOG_ERROR("Save failed.");
        return;
    }

    // Reload level and sync brushes
    m_level->reload(m_renderer);
    sync_brushes();
    LOG_INFO("Level saved and reloaded.");
}

void Editor::delete_selected() {
    if (m_selectedIndex < 0 || m_selectedIndex >= (int)m_brushes.size()) return;
    m_brushes.erase(m_brushes.begin() + m_selectedIndex);
    m_selectedIndex = -1;
}

void Editor::add_brush(BrushType type, ShapeType shape) {
    Brush b;
    b.type = type;
    b.shape = shape;
    b.center = {0,0,0};
    b.size = {4,4,4};
    for (auto& f : b.faces) f = FaceTexture();
    m_brushes.push_back(b);
    m_selectedIndex = (int)m_brushes.size() - 1;
}

void Editor::select_brush(int index) {
    if (index >= 0 && index < (int)m_brushes.size()) {
        m_selectedIndex = index;
    } else {
        m_selectedIndex = -1;
    }
}

// ---------------------------------------------------------------------
// Input handling
// ---------------------------------------------------------------------
void Editor::on_mouse_move(int dx, int dy, bool leftDown, bool middleDown, bool rightDown) {
    if (leftDown) {
        m_editorCamera.orbit((float)dx, (float)dy);
    }
    if (middleDown) {
        m_editorCamera.pan((float)dx, (float)dy);
    }
    (void)rightDown; // not used
}

void Editor::on_mouse_button(int button, bool pressed) {
    // Not used; we rely on the flags passed to on_mouse_move.
    (void)button; (void)pressed;
}

void Editor::on_mouse_wheel(int delta) {
    m_editorCamera.zoom((float)delta);
}

void Editor::on_key_down(int key, bool ctrl, bool shift) {
    // Save
    if (key == m_keybinds.saveKey && ctrl == m_keybinds.saveCtrl && shift == m_keybinds.saveShift) {
        save_level();
        return;
    }
    // Delete
    if (key == m_keybinds.deleteKey && ctrl == m_keybinds.deleteCtrl && shift == m_keybinds.deleteShift) {
        delete_selected();
        return;
    }
}

void Editor::on_key_up(int key) {
    // Not needed
}