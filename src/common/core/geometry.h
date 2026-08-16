#pragma once
#include "math.h"
#include <vector>
#include <string>

enum FaceID : int {
    FACE_FRONT = 0,
    FACE_BACK  = 1,
    FACE_LEFT  = 2,
    FACE_RIGHT = 3,
    FACE_TOP   = 4,
    FACE_BOTTOM = 5,
    NUM_FACES = 6
};

struct FaceTexture
{
    std::string texturePath;
    Vec2 offset;
    Vec2 scale;
    float rotation;
    bool worldLocked;

    FaceTexture() : texturePath("brick.jpg"), offset(0,0), scale(16.0f,16.0f), rotation(0), worldLocked(false) {}
    FaceTexture(const std::string& path, const Vec2& off, const Vec2& sc, float rot, bool locked)
        : texturePath(path), offset(off), scale(sc), rotation(rot), worldLocked(locked) {}
};

struct Vertex { float x,y,z; float u,v; float r,g,b; };
struct MeshData { std::vector<Vertex> vertices; std::vector<unsigned short> indices; };

// ---- Shape generators ----
void generate_box(const Vec3& center, const Vec3& size, const Vec3& color,
                  const FaceTexture faceData[NUM_FACES], bool invertWinding,
                  MeshData& outMainMesh, MeshData& outLabelMesh, bool showLabels = true);

void generate_wedge(const Vec3& center, const Vec3& size, const Vec3& color,
                    const FaceTexture faceData[NUM_FACES], bool invertWinding,
                    MeshData& outMainMesh, MeshData& outLabelMesh, bool showLabels = true);

// ---- Wireframe generators ----
void generate_box_wireframe(const Vec3& center, const Vec3& size, const Vec3& color,
                            std::vector<Vertex>& outLineVerts);

void generate_wedge_wireframe(const Vec3& center, const Vec3& size, const Vec3& color,
                              std::vector<Vertex>& outLineVerts);

// generate_grid_wireframe has been removed