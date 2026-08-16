#include "level.h"
#include "physics/physics_world.h"
#include "world/vmis_format.h"
#include "world/vmis_io.h"        // NEW
#include "core/logger.h"
#include <d3d11.h>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdio>
#include <cmath>
#include <algorithm>

#pragma comment(lib, "d3d11.lib")

// bool Level::load_vm(const char* filepath)
// {
//     brushes.clear();

//     std::ifstream file(filepath);
//     if (!file.is_open()) {
//         LOG_ERROR("Failed to open level file: %s", filepath);
//         return false;
//     }

//     std::string line;
//     int lineNumber = 0;
//     while (std::getline(file, line)) {
//         lineNumber++;
//         size_t start = line.find_first_not_of(" \t");
//         if (start == std::string::npos) continue;
//         if (line[start] == '#') continue;

//         std::istringstream iss(line);
//         std::string typeStr, shapeStr;
//         float x, y, z, w, h, d;
//         std::string texture = "zebra.png";
//         float uScale = 16.0f, vScale = 16.0f;
//         int time = 0;

//         if (!(iss >> typeStr >> shapeStr >> x >> y >> z >> w >> h >> d)) {
//             LOG_WARN("Invalid line %d (expected: type shape x y z w h d ...)", lineNumber);
//             continue;
//         }

//         BrushType brushType;
//         if (typeStr == "add") brushType = BrushType::Add;
//         else if (typeStr == "sub") brushType = BrushType::Sub;
//         else {
//             LOG_WARN("Invalid type '%s' at line %d", typeStr.c_str(), lineNumber);
//             continue;
//         }

//         ShapeType shapeType;
//         if (shapeStr == "box") shapeType = ShapeType::Box;
//         else if (shapeStr == "wedge") shapeType = ShapeType::Wedge;
//         else {
//             LOG_WARN("Invalid shape '%s' at line %d", shapeStr.c_str(), lineNumber);
//             continue;
//         }

//         if (!(iss >> texture)) texture = "zebra.png";
//         if (!(iss >> uScale)) uScale = 16.0f;
//         if (!(iss >> vScale)) vScale = 16.0f;
//         if (!(iss >> time)) time = 0;

//         Brush brush;
//         brush.center = { x, y, z };
//         brush.size = { w, h, d };
//         brush.type = brushType;
//         brush.shape = shapeType;
//         brush.color = { 1.0f, 1.0f, 1.0f };
//         brush.time = time;

//         for (int i = 0; i < NUM_FACES; ++i) {
//             brush.faces[i].texturePath = texture;
//             brush.faces[i].offset = { 0.0f, 0.0f };
//             brush.faces[i].scale = { uScale, vScale };
//             brush.faces[i].rotation = 0.0f;
//             brush.faces[i].worldLocked = false;
//         }

//         brushes.push_back(brush);
//     }

//     file.close();
//     LOG_INFO("Loaded %zu brushes from %s", brushes.size(), filepath);
//     return true;
// }

// bool Level::save_vm(const char* filepath)
// {
//     std::ofstream file(filepath);
//     if (!file.is_open()) {
//         LOG_ERROR("Failed to save level file: %s", filepath);
//         return false;
//     }

//     file << "# Vibe Map (.vm) File\n";
//     file << "# Format: <type> <shape> <x> <y> <z> <width> <height> <depth> [texture] [uScale] [vScale] [time]\n";
//     file << "#   type: add or sub\n";
//     file << "#   shape: box or wedge\n\n";

//     for (const Brush& brush : brushes) {
//         const char* typeStr = (brush.type == BrushType::Sub) ? "sub" : "add";
//         const char* shapeStr = (brush.shape == ShapeType::Wedge) ? "wedge" : "box";
//         const FaceTexture& ft = brush.faces[0];

//         file << typeStr
//              << " " << shapeStr
//              << " " << brush.center.x
//              << " " << brush.center.y
//              << " " << brush.center.z
//              << " " << brush.size.x
//              << " " << brush.size.y
//              << " " << brush.size.z
//              << " " << ft.texturePath
//              << " " << ft.scale.x
//              << " " << ft.scale.y
//              << " " << brush.time
//              << "\n";
//     }

//     file.close();
//     LOG_INFO("Saved %zu brushes to %s", brushes.size(), filepath);
//     return true;
// }

void Level::add_brush(const Brush& brush)
{
    brushes.push_back(brush);
}
// ----------------------------------------------------------------------
// NEW: load_vmis
// ----------------------------------------------------------------------
bool Level::load_vmis(const char* path, Renderer* renderer)
{
    std::vector<Brush> loadedBrushes;
    std::vector<Vertex> verts;
    std::vector<uint32_t> indices;
    std::vector<VMISMaterial> materials;
    std::vector<Triangle> physTris;
    std::vector<Vertex> debugLines;

    if (!read_vmis(path, loadedBrushes, verts, indices, materials, physTris, debugLines)) {
        LOG_ERROR("Failed to read VMIS file: %s", path);
        return false;
    }

    LOG_INFO("Loaded VMIS: %zu brushes, %zu vertices, %zu indices, %zu materials, %zu phys tris",
             loadedBrushes.size(), verts.size(), indices.size(), materials.size(), physTris.size());

    // ---- Store brushes (for editor later) ----
    brushes = std::move(loadedBrushes);

    // ---- Build renderable ----
    ID3D11Device* device = (ID3D11Device*)renderer->get_device();
    if (!device) {
        LOG_ERROR("Renderer device is null!");
        return false;
    }

    if (verts.empty() || indices.empty()) {
        LOG_WARN("VMIS has no geometry, nothing to render.");
        return true; // not an error, just empty level
    }

    // Vertex buffer
    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.ByteWidth = (UINT)(verts.size() * sizeof(Vertex));
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vdata = { verts.data() };
    ComPtr<ID3D11Buffer> vbuf;
    HRESULT hr = device->CreateBuffer(&vbd, &vdata, vbuf.GetAddressOf());
    if (FAILED(hr) || !vbuf) {
        LOG_ERROR("Failed to create vertex buffer (hr=0x%08X)", hr);
        return false;
    }

    // Index buffer
    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = (UINT)(indices.size() * sizeof(uint32_t));
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA idata = { indices.data() };
    ComPtr<ID3D11Buffer> ibuf;
    hr = device->CreateBuffer(&ibd, &idata, ibuf.GetAddressOf());
    if (FAILED(hr) || !ibuf) {
        LOG_ERROR("Failed to create index buffer (hr=0x%08X)", hr);
        return false;
    }

    // ---- Load texture (use first material) ----
    Renderable rend;
    rend.vertexBuffer = vbuf;
    rend.indexBuffer = ibuf;
    rend.indexCount = (int)indices.size();

    if (!materials.empty()) {
        LOG_INFO("Loading texture: %s", materials[0].texturePath);
        rend.textureView = (ID3D11ShaderResourceView*)renderer->load_texture(materials[0].texturePath);
        if (!rend.textureView) {
            LOG_WARN("Texture load failed, using default");
            rend.textureView = (ID3D11ShaderResourceView*)renderer->load_texture("zebra.png");
        }
    } else {
        rend.textureView = (ID3D11ShaderResourceView*)renderer->load_texture("zebra.png");
    }

    renderables.clear();
    renderables.push_back(std::move(rend));

    // ---- Physics ----
    m_collisionTriangles = std::move(physTris);
    m_physicsWorld.build(m_collisionTriangles);

    // ---- Debug mesh ----
    build_debug_mesh(renderer);

    return true;
}

// ---- Build debug mesh ----
void Level::build_debug_mesh(Renderer* renderer)
{
    if (!renderer) return;

    ID3D11Device* device = (ID3D11Device*)renderer->get_device();
    if (!device) return;

    // Clear previous debug data
    m_debugVertices.clear();
    m_debugIndices.clear();
    m_debugLineVertices.clear();

    if (m_collisionTriangles.empty()) return;

    // Generate colors per triangle using golden ratio for nice distribution
    const float goldenRatio = 0.618033988749895f;
    size_t triCount = m_collisionTriangles.size();

    // Expand triangles: each triangle gets its own 3 vertices with unique color
    m_debugVertices.reserve(triCount * 3);
    m_debugIndices.reserve(triCount * 3);

    // For wireframe: each triangle adds 3 line segments (6 vertices) but we'll store lines as linelist (2 vertices per line)
    m_debugLineVertices.reserve(triCount * 6); // 3 edges * 2 vertices

    for (size_t i = 0; i < triCount; ++i) {
        const Triangle& tri = m_collisionTriangles[i];

        // Color from HSV: hue based on index, saturation 0.8, value 0.8
        float hue = fmodf(i * goldenRatio, 1.0f);
        // Convert HSV to RGB (simple)
        float h6 = hue * 6.0f;
        float frac = h6 - floorf(h6);
        float p = 0.8f * (1.0f - 0.8f);
        float q = 0.8f * (1.0f - 0.8f * frac);
        float t = 0.8f * (1.0f - 0.8f * (1.0f - frac));
        float r, g, b;
        int sector = (int)floorf(h6) % 6;
        switch (sector) {
            case 0: r = 0.8f; g = t; b = p; break;
            case 1: r = q; g = 0.8f; b = p; break;
            case 2: r = p; g = 0.8f; b = t; break;
            case 3: r = p; g = q; b = 0.8f; break;
            case 4: r = t; g = p; b = 0.8f; break;
            default: r = 0.8f; g = p; b = q; break;
        }

        // Add 3 vertices for this triangle with the color
        Vec3 verts[3] = { tri.v0, tri.v1, tri.v2 };
        // Compute UVs as dummy (0,0) for debug
        for (int j = 0; j < 3; ++j) {
            Vertex v;
            v.x = verts[j].x;
            v.y = verts[j].y;
            v.z = verts[j].z;
            v.u = 0.0f; v.v = 0.0f;
            v.r = r; v.g = g; v.b = b;
            m_debugVertices.push_back(v);
        }
        unsigned short base = (unsigned short)(i * 3);
        m_debugIndices.push_back(base);
        m_debugIndices.push_back(base + 1);
        m_debugIndices.push_back(base + 2);

        // Wireframe edges (linelist)
        // Edge 0-1
        Vertex lv1 = { verts[0].x, verts[0].y, verts[0].z, 0,0, 1.0f,1.0f,1.0f };
        Vertex lv2 = { verts[1].x, verts[1].y, verts[1].z, 0,0, 1.0f,1.0f,1.0f };
        m_debugLineVertices.push_back(lv1);
        m_debugLineVertices.push_back(lv2);
        // Edge 1-2
        lv1 = { verts[1].x, verts[1].y, verts[1].z, 0,0, 1.0f,1.0f,1.0f };
        lv2 = { verts[2].x, verts[2].y, verts[2].z, 0,0, 1.0f,1.0f,1.0f };
        m_debugLineVertices.push_back(lv1);
        m_debugLineVertices.push_back(lv2);
        // Edge 2-0
        lv1 = { verts[2].x, verts[2].y, verts[2].z, 0,0, 1.0f,1.0f,1.0f };
        lv2 = { verts[0].x, verts[0].y, verts[0].z, 0,0, 1.0f,1.0f,1.0f };
        m_debugLineVertices.push_back(lv1);
        m_debugLineVertices.push_back(lv2);
    }

    // Create vertex buffer for debug triangles
    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.ByteWidth = (UINT)(m_debugVertices.size() * sizeof(Vertex));
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vdata = { m_debugVertices.data() };
    HRESULT hr = device->CreateBuffer(&vbd, &vdata, m_debugRenderable.vertexBuffer.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create debug vertex buffer (hr=0x%08X)", hr);
        return;
    }

    // Index buffer
    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = (UINT)(m_debugIndices.size() * sizeof(unsigned short));
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA idata = { m_debugIndices.data() };
    hr = device->CreateBuffer(&ibd, &idata, m_debugRenderable.indexBuffer.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create debug index buffer (hr=0x%08X)", hr);
        return;
    }
    m_debugRenderable.indexCount = (int)m_debugIndices.size();

    // ---- Wireframe (linelist) ----
    D3D11_BUFFER_DESC lbd = {};
    lbd.Usage = D3D11_USAGE_DEFAULT;
    lbd.ByteWidth = (UINT)(m_debugLineVertices.size() * sizeof(Vertex));
    lbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA ldata = { m_debugLineVertices.data() };
    hr = device->CreateBuffer(&lbd, &ldata, m_debugWireframe.vertexBuffer.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create debug wireframe buffer (hr=0x%08X)", hr);
        return;
    }
    // No index buffer needed for lines
    m_debugWireframe.indexBuffer.Reset();
    m_debugWireframe.indexCount = (int)m_debugLineVertices.size(); // number of vertices for linelist

    LOG_INFO("Debug mesh built: %zu triangles, %zu line vertices", triCount, m_debugLineVertices.size());
}

void Level::set_debug_mode(bool enabled)
{
    if (m_debugMode == enabled) return;
    m_debugMode = enabled;
    LOG_INFO("Debug mode %s", enabled ? "ON" : "OFF");
}

bool Level::build(Renderer* renderer, const char* levelPath) {
    if (!levelPath || levelPath[0] == '\0') {
        LOG_ERROR("No level file specified in settings.");
        return false;
    }
    m_levelPath = levelPath;
    if (load_vmis(levelPath, renderer)) {
        LOG_INFO("Loaded level from %s (VMIS format)", levelPath);
        return true;
    }
    LOG_ERROR("Failed to load level file: %s", levelPath);
    return false;
}


bool Level::reload(Renderer* renderer) {
    if (m_levelPath.empty()) {
        LOG_ERROR("Cannot reload: no level path set.");
        return false;
    }
    // Clear existing renderables
    renderables.clear();
    labelRenderables.clear();
    m_collisionTriangles.clear();

    // Reload
    return build(renderer, m_levelPath.c_str());
}

void Level::render(Renderer* renderer)
{
    ID3D11DeviceContext* context = (ID3D11DeviceContext*)renderer->get_context();
    if (!context) {
        LOG_ERROR("Context is null in render()");
        return;
    }

    UINT stride = sizeof(Vertex);
    UINT offset = 0;

    if (m_debugMode) {
        // ---- Debug rendering: colored triangles + wireframe ----
        renderer->apply_pipeline();
        renderer->set_texture(nullptr); // no texture, use vertex colors

        // Draw colored triangles
        if (m_debugRenderable.vertexBuffer && m_debugRenderable.indexBuffer) {
            context->IASetVertexBuffers(0, 1, m_debugRenderable.vertexBuffer.GetAddressOf(), &stride, &offset);
            context->IASetIndexBuffer(m_debugRenderable.indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
            context->DrawIndexed(m_debugRenderable.indexCount, 0, 0);
        }

        // Draw wireframe (linelist) on top
        if (m_debugWireframe.vertexBuffer) {
            renderer->draw_lines(m_debugWireframe.vertexBuffer.Get(), m_debugWireframe.indexCount);
        }
        return;
    }

    // ---- Normal rendering ----
    renderer->apply_pipeline();

    for (size_t i = 0; i < renderables.size(); ++i) {
        const Renderable& rend = renderables[i];
        if (!rend.vertexBuffer || !rend.indexBuffer) continue;

        renderer->set_texture(rend.textureView.Get());
        context->IASetVertexBuffers(0, 1, rend.vertexBuffer.GetAddressOf(), &stride, &offset);
        context->IASetIndexBuffer(rend.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        context->DrawIndexed(rend.indexCount, 0, 0);
    }

    // ---- Labels ----
    if (!labelRenderables.empty()) {
        renderer->apply_font_pipeline();
        for (const Renderable& labelRend : labelRenderables) {
            context->IASetVertexBuffers(0, 1, labelRend.vertexBuffer.GetAddressOf(), &stride, &offset);
            context->IASetIndexBuffer(labelRend.indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
            context->DrawIndexed(labelRend.indexCount, 0, 0);
        }
        renderer->apply_pipeline();
    }
}

void Level::shutdown(Renderer* renderer)
{
    (void)renderer;
    renderables.clear();
    labelRenderables.clear();
    m_collisionTriangles.clear();

    // Debug resources auto-released by ComPtr
    m_debugVertices.clear();
    m_debugIndices.clear();
    m_debugLineVertices.clear();
}
