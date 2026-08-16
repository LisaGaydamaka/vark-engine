#pragma once
#include <cstdint>
#include <vector>
#include <string>

#pragma pack(push, 1)
struct VMBHeader {
    char magic[4];          // "VMB "
    uint32_t version;       // 1
    uint32_t vertexCount;
    uint32_t indexCount;
    uint32_t materialCount;
    uint32_t physicsTriCount;
};

struct VMBVertex {
    float x, y, z;
    float u, v;
};

struct VMBMaterial {
    char textureName[64];
    float uScale;
    float vScale;
    float rotation;
    float offsetX;
    float offsetY;
    uint8_t worldLocked;
    uint8_t padding[3];
};

struct VMBTriangle {
    uint32_t i0, i1, i2;
    uint32_t materialIndex;
};
#pragma pack(pop)