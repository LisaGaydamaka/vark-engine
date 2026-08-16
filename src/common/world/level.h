#pragma once
#include <vector>
#include <string>
#include <array>
#include <wrl/client.h>
#include "core/math.h"
#include "core/geometry.h"
#include "renderer/renderer.h"
#include "physics/physics_world.h"
// #include "vmb_format.h"   // we can remove this later

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
    Brush() : shape(ShapeType::Box) {
        for (auto& f : faces) f = FaceTexture();
    }
};

class Level {
public:
    bool load_vm(const char* filepath);
    bool save_vm(const char* filepath);
    void add_brush(const Brush& brush);

    bool build(Renderer* renderer, const char* levelPath = nullptr);
    bool load_vmb(const char* path, Renderer* renderer);   // kept for reference
    bool load_vmis(const char* path, Renderer* renderer);  // NEW
    void render(Renderer* renderer);
    void shutdown(Renderer* renderer);

    const std::vector<Brush>& get_brushes() const { return brushes; }
    const PhysicsWorld& get_physics_world() const { return m_physicsWorld; }
    const char* get_level_path() const { return m_levelPath.c_str(); }

    void rebuild(Renderer* renderer) { build(renderer, nullptr); }
    void clear() { brushes.clear(); }

    // ---- Debug mode ----
    void set_debug_mode(bool enabled);
    bool is_debug_mode() const { return m_debugMode; }
    bool reload(Renderer* renderer);   // reload the current level file

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
    std::string m_levelPath;           // store the path used in build()

    // ---- Debug ----
    bool m_debugMode = false;
    Renderable m_debugRenderable;
    Renderable m_debugWireframe;
    std::vector<Vertex> m_debugVertices;
    std::vector<unsigned short> m_debugIndices;
    std::vector<Vertex> m_debugLineVertices;
};