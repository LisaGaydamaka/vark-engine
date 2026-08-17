#include "level.h"
#include "physics/physics_world.h"
#include "world/vmis_format.h"
#include "world/vmis_io.h"
#include "core/logger.h"
#include <d3d11.h>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdio>
#include <cmath>
#include <algorithm>

#pragma comment(lib, "d3d11.lib")

// ----------------------------------------------------------------------
// NEW: normalize_brush_times
// ----------------------------------------------------------------------
void Level::normalize_brush_times() {
    for (size_t i = 0; i < brushes.size(); ++i) {
        brushes[i].time = (int)i;
    }
}

// ----------------------------------------------------------------------
// load_vmis – reads .vmis, normalises times, builds renderables
// ----------------------------------------------------------------------
bool Level::load_vmis(const char* path, Renderer* renderer) {
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

    brushes = std::move(loadedBrushes);

    // ---- Normalise times to sequential order ----
    normalize_brush_times();

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

// ----------------------------------------------------------------------
// build_debug_mesh – unchanged, but uses Renderable correctly
// ----------------------------------------------------------------------
void Level::build_debug_mesh(Renderer* renderer) {
    if (!renderer) return;

    ID3D11Device* device = (ID3D11Device*)renderer->get_device();
    if (!device) return;

    m_debugVertices.clear();
    m_debugIndices.clear();
    m_debugLineVertices.clear();

    if (m_collisionTriangles.empty()) return;

    const float goldenRatio = 0.618033988749895f;
    size_t triCount = m_collisionTriangles.size();

    m_debugVertices.reserve(triCount * 3);
    m_debugIndices.reserve(triCount * 3);
    m_debugLineVertices.reserve(triCount * 6);

    for (size_t i = 0; i < triCount; ++i) {
        const Triangle& tri = m_collisionTriangles[i];

        float hue = fmodf(i * goldenRatio, 1.0f);
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

        Vec3 verts[3] = { tri.v0, tri.v1, tri.v2 };
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

        // Wireframe edges
        Vertex lv1 = { verts[0].x, verts[0].y, verts[0].z, 0,0, 1.0f,1.0f,1.0f };
        Vertex lv2 = { verts[1].x, verts[1].y, verts[1].z, 0,0, 1.0f,1.0f,1.0f };
        m_debugLineVertices.push_back(lv1);
        m_debugLineVertices.push_back(lv2);
        lv1 = { verts[1].x, verts[1].y, verts[1].z, 0,0, 1.0f,1.0f,1.0f };
        lv2 = { verts[2].x, verts[2].y, verts[2].z, 0,0, 1.0f,1.0f,1.0f };
        m_debugLineVertices.push_back(lv1);
        m_debugLineVertices.push_back(lv2);
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

    // Wireframe (linelist)
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
    m_debugWireframe.indexBuffer.Reset();
    m_debugWireframe.indexCount = (int)m_debugLineVertices.size();

    LOG_INFO("Debug mesh built: %zu triangles, %zu line vertices", triCount, m_debugLineVertices.size());
}

// ----------------------------------------------------------------------
// Other Level methods (build, reload, render, shutdown, set_debug_mode)
// ----------------------------------------------------------------------
void Level::set_debug_mode(bool enabled) {
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
    renderables.clear();
    labelRenderables.clear();
    m_collisionTriangles.clear();
    return build(renderer, m_levelPath.c_str());
}

void Level::render(Renderer* renderer) {
    ID3D11DeviceContext* context = (ID3D11DeviceContext*)renderer->get_context();
    if (!context) {
        LOG_ERROR("Context is null in render()");
        return;
    }

    UINT stride = sizeof(Vertex);
    UINT offset = 0;

    if (m_debugMode) {
        renderer->apply_pipeline();
        renderer->set_texture(nullptr);

        if (m_debugRenderable.vertexBuffer && m_debugRenderable.indexBuffer) {
            ID3D11Buffer* vb = m_debugRenderable.vertexBuffer.Get();
            context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
            context->IASetIndexBuffer(m_debugRenderable.indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
            context->DrawIndexed(m_debugRenderable.indexCount, 0, 0);
        }

        if (m_debugWireframe.vertexBuffer) {
            renderer->draw_lines(m_debugWireframe.vertexBuffer.Get(), m_debugWireframe.indexCount);
        }
        return;
    }

    // Normal rendering
    renderer->apply_pipeline();

    for (size_t i = 0; i < renderables.size(); ++i) {
        const Renderable& rend = renderables[i];
        if (!rend.vertexBuffer || !rend.indexBuffer) continue;

        renderer->set_texture(rend.textureView.Get());
        ID3D11Buffer* vb = rend.vertexBuffer.Get();
        context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
        context->IASetIndexBuffer(rend.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        context->DrawIndexed(rend.indexCount, 0, 0);
    }

    // Labels
    if (!labelRenderables.empty()) {
        renderer->apply_font_pipeline();
        for (const Renderable& labelRend : labelRenderables) {
            ID3D11Buffer* vb = labelRend.vertexBuffer.Get();
            context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
            context->IASetIndexBuffer(labelRend.indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
            context->DrawIndexed(labelRend.indexCount, 0, 0);
        }
        renderer->apply_pipeline();
    }
}

void Level::shutdown(Renderer* renderer) {
    (void)renderer;
    renderables.clear();
    labelRenderables.clear();
    m_collisionTriangles.clear();
    m_debugVertices.clear();
    m_debugIndices.clear();
    m_debugLineVertices.clear();
}