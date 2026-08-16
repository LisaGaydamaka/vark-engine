#pragma once
#include "core/math.h"
#include "world/level.h"   // for Brush, FaceTexture, etc.

struct CSGVertex {
    Vec3 pos;
    Vec2 uv;
};

struct CSGPoly {
    std::vector<CSGVertex> verts;
    Vec3 normal;
    FaceTexture texture;   // copy of the face texture
    bool worldLocked;
};

struct Plane {
    Vec3 n;
    float d;
};