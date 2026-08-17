// vmis_format.h
#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "core/math.h"
#include "world/level.h"  // for Brush, Vertex, etc.

#define VMIS_VERSION 2    // <-- new version for name field

#pragma pack(push, 1)

// -------- Header (always at file start) --------
struct VMISHeader {
    char magic[4];          // "VMIS"
    uint32_t version;       // 1 or 2
    uint32_t fileSize;      // total file size (for validation)
    uint64_t offsetBrushCatalog;
    uint64_t offsetBakedMesh;
    uint64_t offsetMaterialTable;
    uint64_t offsetPhysicsMesh;
    uint64_t offsetDebugWireframe;  // 0 if absent
};

// -------- Brush Catalog (exact copy of editor's brush list) --------
// We store each brush as a binary record.
struct VMISBrush {
    float center[3];
    float size[3];
    uint8_t type;           // 0=Add, 1=Sub
    uint8_t shape;          // 0=Box, 1=Wedge
    uint8_t padding[2];
    int32_t time;
    char name[64];          // <-- new field for brush name
    struct FaceRecord {
        char texturePath[64];
        float offsetX, offsetY;
        float scaleX, scaleY;
        float rotation;
        uint8_t worldLocked;
        uint8_t padding[3];
    } faces[6];
};

// -------- Baked Mesh (renderable triangles) --------
struct VMISVertex {
    float x, y, z;
    float u, v;
    float r, g, b;      // we keep vertex colors (optional)
};

// -------- Material Table (deduplicated textures) --------
struct VMISMaterial {
    char texturePath[64];
    float scaleX, scaleY;
    float offsetX, offsetY;
    float rotation;
    uint8_t worldLocked;
    uint8_t padding[3];
};

// Physics Mesh: same as baked mesh but stored separately (list of triangles)
// We can reuse VMISVertex for vertices, and index list as uint32_t.

// Debug Wireframe: line list vertices (just position + color) – optional.

#pragma pack(pop)