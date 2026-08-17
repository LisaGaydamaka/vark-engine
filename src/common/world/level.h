// src/common/world/level.h
#pragma once
#include <vector>
#include <string>          // <-- added for std::string
#include <array>
#include <wrl/client.h>
#include "core/math.h"
#include "core/geometry.h"
#include "renderer/renderer.h"
#include "physics/physics_world.h"

using Microsoft::WRL::ComPtr;

enum class BrushType { Add, Sub };
enum class ShapeType { Box, Wedge };

struct Brush {
    Vec3 center;
    Vec3 size;
    BrushType type;
    ShapeType shape;
    Vec3 color;
    std::array<FaceTexture, NUM_FACES> faces;
    int time = 0;
    std::string name;          // <-- new member

    Brush() : shape(ShapeType::Box) {
        for (auto& f : faces) f = FaceTexture();
        name = "Brush";        // default name, will be overwritten
    }
};

class Level {
public:
    bool build(Renderer* renderer, const char* levelPath = nullptr);
    bool load_vmis(const char* path, Renderer* renderer);
    void render(Renderer* renderer);
    void shutdown(Renderer* renderer);

    const std::vector<Brush>& get_brushes() const { return brushes; }
    const PhysicsWorld& get_physics_world() const { return m_physicsWorld; }
    const char* get_level_path() const { return m_levelPath.c_str(); }

    void rebuild(Renderer* renderer) { build(renderer, nullptr); }
    void clear() { brushes.clear(); }

    void set_debug_mode(bool enabled);
    bool is_debug_mode() const { return m_debugMode; }
    bool reload(Renderer* renderer);

private:
    struct Renderable {
        ComPtr<ID3D11Buffer> vertexBuffer;
        ComPtr<ID3D11Buffer> indexBuffer;
        int indexCount = 0;
        ComPtr<ID3D11ShaderResourceView> textureView;
    };

    void build_debug_mesh(Renderer* renderer);

    std::vector<Brush> brushes;
    std::vector<Renderable> renderables;
    std::vector<Renderable> labelRenderables;
    PhysicsWorld m_physicsWorld;
    std::vector<Triangle> m_collisionTriangles;
    std::string m_levelPath;

    bool m_debugMode = false;
    Renderable m_debugRenderable;
    Renderable m_debugWireframe;
    std::vector<Vertex> m_debugVertices;
    std::vector<unsigned short> m_debugIndices;
    std::vector<Vertex> m_debugLineVertices;
};