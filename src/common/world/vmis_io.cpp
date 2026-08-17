// vmis_io.cpp
#include "vmis_io.h"
#include "core/logger.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <vector>

// -----------------------------------------------------------------------------
// Helper: copy string into fixed char array with truncation
// -----------------------------------------------------------------------------
static void copy_string(char* dest, size_t destSize, const std::string& src) {
    size_t len = src.copy(dest, destSize - 1);
    dest[len] = '\0';
}

// -----------------------------------------------------------------------------
// Convert a Brush to VMISBrush (binary record)
// -----------------------------------------------------------------------------
static VMISBrush brush_to_vmisbrush(const Brush& brush) {
    VMISBrush vb{};
    vb.center[0] = brush.center.x;
    vb.center[1] = brush.center.y;
    vb.center[2] = brush.center.z;
    vb.size[0] = brush.size.x;
    vb.size[1] = brush.size.y;
    vb.size[2] = brush.size.z;
    vb.type = (brush.type == BrushType::Add) ? 0 : 1;
    vb.shape = (brush.shape == ShapeType::Box) ? 0 : 1;
    vb.time = brush.time;

    for (int i = 0; i < 6; ++i) {
        const auto& ft = brush.faces[i];
        copy_string(vb.faces[i].texturePath, sizeof(vb.faces[i].texturePath), ft.texturePath);
        vb.faces[i].offsetX = ft.offset.x;
        vb.faces[i].offsetY = ft.offset.y;
        vb.faces[i].scaleX = ft.scale.x;
        vb.faces[i].scaleY = ft.scale.y;
        vb.faces[i].rotation = ft.rotation;
        vb.faces[i].worldLocked = ft.worldLocked ? 1 : 0;
    }
    return vb;
}

// -----------------------------------------------------------------------------
// Convert VMISBrush back to Brush
// -----------------------------------------------------------------------------
static Brush vmisbrush_to_brush(const VMISBrush& vb) {
    Brush brush;
    brush.center = { vb.center[0], vb.center[1], vb.center[2] };
    brush.size = { vb.size[0], vb.size[1], vb.size[2] };
    brush.type = (vb.type == 0) ? BrushType::Add : BrushType::Sub;
    brush.shape = (vb.shape == 0) ? ShapeType::Box : ShapeType::Wedge;
    brush.time = vb.time;
    brush.color = { 1.0f, 1.0f, 1.0f };

    for (int i = 0; i < 6; ++i) {
        const auto& vf = vb.faces[i];
        auto& ft = brush.faces[i];
        ft.texturePath = vf.texturePath;
        ft.offset = { vf.offsetX, vf.offsetY };
        ft.scale = { vf.scaleX, vf.scaleY };
        ft.rotation = vf.rotation;
        ft.worldLocked = (vf.worldLocked != 0);
    }

    // ---- Set default name based on type and shape ----
    std::string typeStr = (brush.type == BrushType::Add) ? "Add" : "Sub";
    std::string shapeStr = (brush.shape == ShapeType::Box) ? "Box" : "Wedge";
    brush.name = typeStr + " " + shapeStr;

    return brush;
}

// -----------------------------------------------------------------------------
// Write .vmis file
// -----------------------------------------------------------------------------
bool write_vmis(const char* path,
                const std::vector<Brush>& brushes,
                const std::vector<Vertex>& bakedVerts,
                const std::vector<uint32_t>& bakedIndices,
                const std::vector<VMISMaterial>& materials,
                const std::vector<Triangle>& physicsTris,
                const std::vector<Vertex>& debugLines)
{
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        LOG_ERROR("write_vmis: cannot open '%s' for writing", path);
        return false;
    }

    // ---------- Helper: write a section with size prefix ----------
    auto write_section = [&](const void* data, size_t byteSize) -> uint64_t {
        uint64_t offset = file.tellp();
        file.write(reinterpret_cast<const char*>(data), byteSize);
        return offset;
    };

    // ---------- 1. Prepare sections ----------
    // Brush Catalog: convert brushes to VMISBrush
    std::vector<VMISBrush> vmisBrushes;
    vmisBrushes.reserve(brushes.size());
    for (const auto& b : brushes) {
        vmisBrushes.push_back(brush_to_vmisbrush(b));
    }
    size_t brushCatalogSize = vmisBrushes.size() * sizeof(VMISBrush);

    // Baked Mesh: we store vertex count, then vertices, then index count, then indices.
    // We'll pack as: [uint32_t numVerts][verts][uint32_t numIndices][indices]
    size_t bakedVertsSize = bakedVerts.size() * sizeof(Vertex);
    size_t bakedIndicesSize = bakedIndices.size() * sizeof(uint32_t);
    size_t bakedMeshTotal = sizeof(uint32_t) + bakedVertsSize + sizeof(uint32_t) + bakedIndicesSize;

    // Material Table: store count then array
    size_t matTableSize = sizeof(uint32_t) + materials.size() * sizeof(VMISMaterial);

    // Physics Mesh: store triangle count then triangles (each triangle = 3 Vec3)
    size_t physicsSize = sizeof(uint32_t) + physicsTris.size() * sizeof(Triangle);

    // Debug Wireframe: store line vertex count then vertices
    size_t debugSize = sizeof(uint32_t) + debugLines.size() * sizeof(Vertex);

    // ---------- 2. Write header (temporary) ----------
    VMISHeader header{};
    memcpy(header.magic, "VMIS", 4);
    header.version = 1;
    header.fileSize = 0; // placeholder

    // We'll fill offsets after we know them.
    // For now, skip header space.
    file.seekp(0);
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // ---------- 3. Write each section, recording offsets ----------
    // Brush Catalog
    uint64_t offBrushCatalog = file.tellp();
    file.write(reinterpret_cast<const char*>(vmisBrushes.data()), brushCatalogSize);

    // Baked Mesh
    uint64_t offBakedMesh = file.tellp();
    uint32_t numVerts = (uint32_t)bakedVerts.size();
    file.write(reinterpret_cast<const char*>(&numVerts), sizeof(numVerts));
    file.write(reinterpret_cast<const char*>(bakedVerts.data()), bakedVertsSize);
    uint32_t numIndices = (uint32_t)bakedIndices.size();
    file.write(reinterpret_cast<const char*>(&numIndices), sizeof(numIndices));
    file.write(reinterpret_cast<const char*>(bakedIndices.data()), bakedIndicesSize);

    // Material Table
    uint64_t offMatTable = file.tellp();
    uint32_t numMats = (uint32_t)materials.size();
    file.write(reinterpret_cast<const char*>(&numMats), sizeof(numMats));
    file.write(reinterpret_cast<const char*>(materials.data()), materials.size() * sizeof(VMISMaterial));

    // Physics Mesh
    uint64_t offPhysics = file.tellp();
    uint32_t numPhysTris = (uint32_t)physicsTris.size();
    file.write(reinterpret_cast<const char*>(&numPhysTris), sizeof(numPhysTris));
    file.write(reinterpret_cast<const char*>(physicsTris.data()), physicsTris.size() * sizeof(Triangle));

    // Debug Wireframe
    uint64_t offDebug = file.tellp();
    uint32_t numDebugVerts = (uint32_t)debugLines.size();
    file.write(reinterpret_cast<const char*>(&numDebugVerts), sizeof(numDebugVerts));
    file.write(reinterpret_cast<const char*>(debugLines.data()), debugLines.size() * sizeof(Vertex));

    // ---------- 4. Finalise header ----------
    uint64_t endPos = file.tellp();
    header.fileSize = (uint32_t)endPos;  // fits in 32-bit (file < 4GB)
    header.offsetBrushCatalog = offBrushCatalog;
    header.offsetBakedMesh = offBakedMesh;
    header.offsetMaterialTable = offMatTable;
    header.offsetPhysicsMesh = offPhysics;
    header.offsetDebugWireframe = (debugLines.empty() ? 0 : offDebug);

    file.seekp(0);
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));

    file.close();
    LOG_INFO("VMIS written: %s (%.2f KB)", path, endPos / 1024.0f);
    return true;
}

// -----------------------------------------------------------------------------
// Read .vmis file
// -----------------------------------------------------------------------------
bool read_vmis(const char* path,
               std::vector<Brush>& outBrushes,
               std::vector<Vertex>& outBakedVerts,
               std::vector<uint32_t>& outBakedIndices,
               std::vector<VMISMaterial>& outMaterials,
               std::vector<Triangle>& outPhysicsTris,
               std::vector<Vertex>& outDebugLines)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        LOG_ERROR("read_vmis: cannot open '%s'", path);
        return false;
    }

    // Read header
    VMISHeader header{};
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!file || memcmp(header.magic, "VMIS", 4) != 0) {
        LOG_ERROR("read_vmis: invalid magic (not a VMIS file)");
        return false;
    }
    if (header.version != 1) {
        LOG_ERROR("read_vmis: unsupported version %u", header.version);
        return false;
    }

    // Helper: seek and read a section
    auto read_section = [&](uint64_t offset, void* data, size_t size) -> bool {
        if (offset == 0) {
            // Section may be absent (e.g., debug wireframe)
            return true;
        }
        file.seekg(offset);
        file.read(reinterpret_cast<char*>(data), size);
        return !file.fail();
    };

    // ---------- 1. Brush Catalog ----------
    if (header.offsetBrushCatalog != 0) {
        file.seekg(header.offsetBrushCatalog);
        // We don't know how many brushes; we'll read until end of section.
        // But we can infer from file size? Better: read all remaining bytes and divide by sizeof(VMISBrush)
        // However we don't have a count in the header. We'll read the entire section by jumping to next section.
        // We can compute size from other offsets: next section offset - current.
        uint64_t nextOffset = header.offsetBakedMesh;
        size_t byteSize = (size_t)(nextOffset - header.offsetBrushCatalog);
        if (byteSize % sizeof(VMISBrush) != 0) {
            LOG_ERROR("read_vmis: Brush Catalog size is not a multiple of VMISBrush");
            return false;
        }
        size_t count = byteSize / sizeof(VMISBrush);
        std::vector<VMISBrush> vmisBrushes(count);
        file.read(reinterpret_cast<char*>(vmisBrushes.data()), byteSize);
        if (!file) {
            LOG_ERROR("read_vmis: failed to read Brush Catalog");
            return false;
        }
        outBrushes.clear();
        outBrushes.reserve(count);
        for (const auto& vb : vmisBrushes) {
            outBrushes.push_back(vmisbrush_to_brush(vb));
        }
    } else {
        outBrushes.clear();
    }

    // ---------- 2. Baked Mesh ----------
    if (header.offsetBakedMesh != 0) {
        file.seekg(header.offsetBakedMesh);
        uint32_t numVerts, numIndices;
        file.read(reinterpret_cast<char*>(&numVerts), sizeof(numVerts));
        outBakedVerts.resize(numVerts);
        file.read(reinterpret_cast<char*>(outBakedVerts.data()), numVerts * sizeof(Vertex));
        file.read(reinterpret_cast<char*>(&numIndices), sizeof(numIndices));
        outBakedIndices.resize(numIndices);
        file.read(reinterpret_cast<char*>(outBakedIndices.data()), numIndices * sizeof(uint32_t));
        if (!file) {
            LOG_ERROR("read_vmis: failed to read Baked Mesh");
            return false;
        }
    } else {
        outBakedVerts.clear();
        outBakedIndices.clear();
    }

    // ---------- 3. Material Table ----------
    if (header.offsetMaterialTable != 0) {
        file.seekg(header.offsetMaterialTable);
        uint32_t numMats;
        file.read(reinterpret_cast<char*>(&numMats), sizeof(numMats));
        outMaterials.resize(numMats);
        file.read(reinterpret_cast<char*>(outMaterials.data()), numMats * sizeof(VMISMaterial));
        if (!file) {
            LOG_ERROR("read_vmis: failed to read Material Table");
            return false;
        }
    } else {
        outMaterials.clear();
    }

    // ---------- 4. Physics Mesh ----------
    if (header.offsetPhysicsMesh != 0) {
        file.seekg(header.offsetPhysicsMesh);
        uint32_t numPhysTris;
        file.read(reinterpret_cast<char*>(&numPhysTris), sizeof(numPhysTris));
        outPhysicsTris.resize(numPhysTris);
        file.read(reinterpret_cast<char*>(outPhysicsTris.data()), numPhysTris * sizeof(Triangle));
        if (!file) {
            LOG_ERROR("read_vmis: failed to read Physics Mesh");
            return false;
        }
    } else {
        outPhysicsTris.clear();
    }

    // ---------- 5. Debug Wireframe ----------
    if (header.offsetDebugWireframe != 0) {
        file.seekg(header.offsetDebugWireframe);
        uint32_t numDebugVerts;
        file.read(reinterpret_cast<char*>(&numDebugVerts), sizeof(numDebugVerts));
        outDebugLines.resize(numDebugVerts);
        file.read(reinterpret_cast<char*>(outDebugLines.data()), numDebugVerts * sizeof(Vertex));
        if (!file) {
            LOG_ERROR("read_vmis: failed to read Debug Wireframe");
            return false;
        }
    } else {
        outDebugLines.clear();
    }

    LOG_INFO("VMIS read: %s (%zu brushes, %zu vertices, %zu indices, %zu materials, %zu physics tris, %zu debug verts)",
             path, outBrushes.size(), outBakedVerts.size(), outBakedIndices.size(),
             outMaterials.size(), outPhysicsTris.size(), outDebugLines.size());
    return true;
}