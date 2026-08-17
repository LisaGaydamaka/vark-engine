// src/editor/editor.cpp
#include "editor.h"
#include "editor_ui.h"
#include "core/logger.h"
#include "core/geometry.h"
#include "compiler/csg_solver.h"
#include "world/vmis_io.h"
#include "world/vmis_format.h"
#include <d3d11.h>
#include <algorithm>
#include <unordered_map>
#include <string>
#include <vector>
#include <cfloat>
#include <cstring>

Editor::Editor() {}
Editor::~Editor() { shutdown(); }

bool Editor::initialize(Renderer* renderer, Level* level) {
    m_renderer = renderer;
    m_level = level;

    if (!m_renderer || !m_level) return false;

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
    renumber_times();   // <-- ensure sequential times
    if (m_selectedIndex >= (int)m_brushes.size()) {
        m_selectedIndex = -1;
    }
}

void Editor::update(float dt) {
    (void)dt;
}

void Editor::render() {
    if (!m_initialized) return;

    rebuild_wireframe_buffer();
    if (m_wireframeBuffer && m_wireframeVertexCount > 0) {
        m_renderer->draw_lines(m_wireframeBuffer.Get(), m_wireframeVertexCount);
    }
}

void Editor::rebuild_wireframe_buffer() {
    if (!m_wireframeBuffer) return;

    std::vector<Vertex> lineVerts;
    lineVerts.reserve(m_brushes.size() * 48);

    for (size_t i = 0; i < m_brushes.size(); ++i) {
        const auto& b = m_brushes[i];
        Vec3 color = (i == (size_t)m_selectedIndex) ? Vec3{0.0f, 1.0f, 0.0f} : Vec3{0.0f, 0.0f, 0.0f};
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

int Editor::pick_brush(int mouseX, int mouseY) {
    if (!m_initialized || m_brushes.empty()) return -1;

    Vec3 rayOrigin = m_editorCamera.get_camera()->get_position();
    Vec3 rayDir = screen_to_world_ray(mouseX, mouseY);

    int bestIdx = -1;
    float bestT = FLT_MAX;

    for (size_t i = 0; i < m_brushes.size(); ++i) {
        const auto& b = m_brushes[i];
        Vec3 half = b.size * 0.5f;
        Vec3 min = b.center - half;
        Vec3 max = b.center + half;

        float t;
        if (intersect_aabb(rayOrigin, rayDir, min, max, t)) {
            if (t < bestT) {
                bestT = t;
                bestIdx = (int)i;
            }
        }
    }
    return bestIdx;
}

Vec3 Editor::screen_to_world_ray(int mouseX, int mouseY) {
    int width = m_renderer ? m_renderer->get_width() : 1280;
    int height = m_renderer ? m_renderer->get_height() : 720;
    if (width == 0 || height == 0) return {0,0,0};

    float ndcX = (2.0f * mouseX / width) - 1.0f;
    float ndcY = 1.0f - (2.0f * mouseY / height);

    Camera* cam = m_editorCamera.get_camera();
    Mat4 view = cam->get_view_matrix();
    float aspect = (float)width / (float)height;
    Mat4 proj = mat4_perspective(60.0f * 3.14159265f / 180.0f, aspect, 0.1f, 1000.0f);

    Vec3 forward = cam->get_forward();
    Vec3 right = cam->get_right();
    Vec3 up = cam->get_up();
    Vec3 dir = forward + right * ndcX + up * ndcY;
    dir = dir.normalized();
    return dir;
}

bool Editor::intersect_aabb(const Vec3& rayOrigin, const Vec3& rayDir, const Vec3& boxMin, const Vec3& boxMax, float& outT) const {
    float tMin = -FLT_MAX, tMax = FLT_MAX;
    for (int i = 0; i < 3; ++i) {
        float origin = (i == 0) ? rayOrigin.x : (i == 1) ? rayOrigin.y : rayOrigin.z;
        float dir = (i == 0) ? rayDir.x : (i == 1) ? rayDir.y : rayDir.z;
        float min = (i == 0) ? boxMin.x : (i == 1) ? boxMin.y : boxMin.z;
        float max = (i == 0) ? boxMax.x : (i == 1) ? boxMax.y : boxMax.z;

        if (fabsf(dir) < 1e-6f) {
            if (origin < min || origin > max) return false;
        } else {
            float t1 = (min - origin) / dir;
            float t2 = (max - origin) / dir;
            if (t1 > t2) std::swap(t1, t2);
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMin > tMax) return false;
        }
    }
    outT = tMin;
    return true;
}

void Editor::save_level() {
    if (m_brushes.empty()) {
        LOG_WARN("No brushes to save.");
        return;
    }
    renumber_times();

    // Build material table (VMISMaterial) and indices
    std::vector<VMISMaterial> materials;
    std::unordered_map<std::string, int> texToIndex;
    std::vector<int> materialIndices;
    materialIndices.reserve(m_brushes.size());

    for (const auto& brush : m_brushes) {
        const FaceTexture& ft = brush.faces[0];
        std::string key = ft.texturePath + "|" + std::to_string(ft.scale.x) + "|" + std::to_string(ft.scale.y);
        auto it = texToIndex.find(key);
        int matIdx;
        if (it == texToIndex.end()) {
            VMISMaterial mat;
            strcpy_s(mat.texturePath, sizeof(mat.texturePath), ft.texturePath.c_str());
            mat.scaleX = ft.scale.x;
            mat.scaleY = ft.scale.y;
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

    // Run CSG with VMISMaterial
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

    // Write VMIS using the already built materials
    const char* levelPath = m_level->get_level_path();
    if (!levelPath || !levelPath[0]) {
        LOG_ERROR("No level path set; cannot save.");
        return;
    }
    if (!write_vmis(levelPath, m_brushes, bakedVerts, bakedIndices, materials, physTris, {})) {
        LOG_ERROR("Save failed.");
        return;
    }

    m_level->reload(m_renderer);
    sync_brushes();
    LOG_INFO("Level saved and reloaded.");
}

void Editor::delete_selected() {
    if (m_selectedIndex < 0 || m_selectedIndex >= (int)m_brushes.size()) return;
    m_brushes.erase(m_brushes.begin() + m_selectedIndex);
    m_selectedIndex = -1;
}

// ------ MODIFIED add_brush ------
void Editor::add_brush(BrushType type, ShapeType shape) {
    Brush b;
    b.type = type;
    b.shape = shape;
    b.center = {0,0,0};
    b.size = {4,4,4};
    for (auto& f : b.faces) f = FaceTexture();

    b.time = (int)m_brushes.size();

    // Set name based on type and shape (no numbering)
    std::string typeStr = (type == BrushType::Add) ? "Add" : "Sub";
    std::string shapeStr = (shape == ShapeType::Box) ? "Box" : "Wedge";
    b.name = typeStr + " " + shapeStr;

    m_brushes.push_back(b);
    m_selectedIndex = (int)m_brushes.size() - 1;
}
// ---------------------------------

void Editor::select_brush(int index) {
    if (index >= 0 && index < (int)m_brushes.size()) {
        m_selectedIndex = index;
    } else {
        m_selectedIndex = -1;
    }
}

void Editor::apply_brush_edit(int field, float value) {
    if (m_selectedIndex < 0 || m_selectedIndex >= (int)m_brushes.size()) return;
    auto& b = m_brushes[m_selectedIndex];
    switch ((EditField)field) {
        case EditField::PosX: b.center.x = value; break;
        case EditField::PosY: b.center.y = value; break;
        case EditField::PosZ: b.center.z = value; break;
        case EditField::SizeX: b.size.x = std::max(value, 0.1f); break;
        case EditField::SizeY: b.size.y = std::max(value, 0.1f); break;
        case EditField::SizeZ: b.size.z = std::max(value, 0.1f); break;
        default: break;
    }
}

void Editor::on_mouse_move(int dx, int dy, bool leftDown, bool middleDown, bool rightDown, int modMask) {
    MouseButton downButton = MouseButton::None;
    if (leftDown) downButton = MouseButton::Left;
    else if (middleDown) downButton = MouseButton::Middle;
    else if (rightDown) downButton = MouseButton::Right;
    else return;

    if (downButton == m_keybinds.orbitButton && (modMask & 0x7) == m_keybinds.orbitModifier) {
        m_editorCamera.orbit((float)dx, (float)dy);
        return;
    }
    if (downButton == m_keybinds.panButton && (modMask & 0x7) == m_keybinds.panModifier) {
        m_editorCamera.pan((float)dx, (float)dy);
        return;
    }
}

void Editor::on_mouse_wheel(int delta) {
    m_editorCamera.zoom((float)delta);
}

void Editor::on_key_down(int key, bool ctrl, bool shift) {
    if (key == m_keybinds.saveKey && ctrl == m_keybinds.saveCtrl && shift == m_keybinds.saveShift) {
        save_level();
        return;
    }
    if (key == m_keybinds.deleteKey && ctrl == m_keybinds.deleteCtrl && shift == m_keybinds.deleteShift) {
        delete_selected();
        return;
    }
}

void Editor::set_keybinds(const EditorKeybindSettings& keybinds) {
    m_keybinds = keybinds;
}

void Editor::set_brush_name(int index, const std::string& name) {
    if (index < 0 || index >= (int)m_brushes.size()) return;
    m_brushes[index].name = name;
}

void Editor::renumber_times() {
    if (m_brushes.empty()) return;
    // Sort by current time (which may have gaps)
    std::sort(m_brushes.begin(), m_brushes.end(),
        [](const Brush& a, const Brush& b) {
            return a.time < b.time;
        });
    // Assign sequential times starting from 0
    for (int i = 0; i < (int)m_brushes.size(); ++i) {
        m_brushes[i].time = i;
    }
}