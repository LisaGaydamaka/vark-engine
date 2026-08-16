#undef min
#undef max

#include "csg_solver.h"
#include "core/geometry.h"
#include "world/vmis_format.h"
#include "world/vmis_io.h"
#include <manifold/manifold.h>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <vector>
#include <unordered_map>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

// ----------------------------------------------------------------------
// Convert a brush to manifold::MeshGL, embedding UVs and material index
// ----------------------------------------------------------------------
static manifold::MeshGL brush_to_meshgl(const Brush& brush, int materialIndex) {
    MeshData mesh, dummy;
    if (brush.shape == ShapeType::Box) {
        generate_box(brush.center, brush.size, {1,1,1},
                     brush.faces.data(), false, mesh, dummy, false);
    } else {
        generate_wedge(brush.center, brush.size, {1,1,1},
                       brush.faces.data(), false, mesh, dummy, false);
    }

    manifold::MeshGL gl;
    gl.numProp = 5; // x, y, z, u, v
    gl.vertProperties.reserve(mesh.vertices.size() * 5);
    for (const auto& v : mesh.vertices) {
        gl.vertProperties.push_back(v.x);
        gl.vertProperties.push_back(v.y);
        gl.vertProperties.push_back(v.z);
        gl.vertProperties.push_back(v.u);
        gl.vertProperties.push_back(v.v);
    }

    gl.triVerts.reserve(mesh.indices.size());
    for (auto idx : mesh.indices) {
        gl.triVerts.push_back(static_cast<uint32_t>(idx));
    }

    gl.faceID.resize(mesh.indices.size() / 3, static_cast<uint32_t>(materialIndex));
    gl.Merge();
    return gl;
}

static manifold::Manifold brush_to_manifold(const Brush& brush, int materialIndex) {
    manifold::MeshGL gl = brush_to_meshgl(brush, materialIndex);
    manifold::Manifold m(gl);
    if (m.Status() != manifold::Manifold::Error::NoError) {
        printf("[WARN] Brush manifold status %d\n", (int)m.Status());
    }
    return m;
}

// ----------------------------------------------------------------------
// Convert manifold::Manifold to CSGPoly list, reading UVs and material index
// ----------------------------------------------------------------------
static std::vector<CSGPoly> manifold_to_csgpolys(
    const manifold::Manifold& m,
    const std::vector<VMISMaterial>& materials)   // now VMISMaterial
{
    manifold::MeshGL gl = m.GetMeshGL();
    std::vector<CSGPoly> polys;

    for (size_t t = 0; t < gl.NumTri(); ++t) {
        auto tri = gl.GetTriVerts(t);
        CSGPoly poly;
        for (int i = 0; i < 3; ++i) {
            uint32_t idx = tri[i];
            size_t base = idx * gl.numProp;
            float x = gl.vertProperties[base + 0];
            float y = gl.vertProperties[base + 1];
            float z = gl.vertProperties[base + 2];
            float u = gl.vertProperties[base + 3];
            float v = gl.vertProperties[base + 4];
            poly.verts.push_back({{x, y, z}, {u, v}});
        }
        Vec3 e1 = poly.verts[1].pos - poly.verts[0].pos;
        Vec3 e2 = poly.verts[2].pos - poly.verts[0].pos;
        poly.normal = Vec3::cross(e1, e2).normalized();

        uint32_t matIdx = gl.faceID[t];
        if (matIdx < materials.size()) {
            const auto& mat = materials[matIdx];
            poly.texture.texturePath = mat.texturePath;
            poly.texture.scale = {mat.scaleX, mat.scaleY};
            poly.texture.offset = {mat.offsetX, mat.offsetY};
            poly.texture.rotation = mat.rotation;
            poly.worldLocked = mat.worldLocked != 0;
        } else {
            poly.texture.texturePath = "zebra.png";
            poly.texture.scale = {16.0f, 16.0f};
            poly.worldLocked = false;
        }
        polys.push_back(poly);
    }
    return polys;
}

// ----------------------------------------------------------------------
// Export to OBJ (wavefront)
// ----------------------------------------------------------------------
void export_OBJ(const std::vector<CSGPoly>& polys, const char* path) {
    FILE* f = nullptr;
    fopen_s(&f, path, "w");
    if (!f) {
        fprintf(stderr, "[ERROR] Failed to open OBJ file for writing: %s\n", path);
        return;
    }

    fprintf(f, "# Exported from Vark Engine CSG\n");
    fprintf(f, "o CSGResult\n");

    std::vector<Vec3> positions;
    std::vector<Vec2> uvs;
    std::vector<Vec3> normals;
    struct TriIndices { int posIdx, uvIdx, normIdx; };
    std::vector<TriIndices> triangles;

    for (const auto& poly : polys) {
        size_t n = poly.verts.size();
        if (n < 3) continue;
        Vec3 normal = poly.normal.normalized();
        if (normal.length() < 0.001f) {
            Vec3 e1 = poly.verts[1].pos - poly.verts[0].pos;
            Vec3 e2 = poly.verts[2].pos - poly.verts[0].pos;
            normal = Vec3::cross(e1, e2).normalized();
        }

        for (size_t i = 1; i < n - 1; ++i) {
            for (int j = 0; j < 3; ++j) {
                int idx = (j == 0) ? 0 : (j == 1) ? (int)i : (int)(i + 1);
                positions.push_back(poly.verts[idx].pos);
                uvs.push_back(poly.verts[idx].uv);
                normals.push_back(normal);
                triangles.push_back({ (int)positions.size() - 1,
                                      (int)uvs.size() - 1,
                                      (int)normals.size() - 1 });
            }
        }
    }

    for (const auto& p : positions) fprintf(f, "v %f %f %f\n", p.x, p.y, p.z);
    for (const auto& uv : uvs) fprintf(f, "vt %f %f\n", uv.x, uv.y);
    for (const auto& n : normals) fprintf(f, "vn %f %f %f\n", n.x, n.y, n.z);

    for (size_t t = 0; t < triangles.size(); t += 3) {
        fprintf(f, "f %d/%d/%d %d/%d/%d %d/%d/%d\n",
            triangles[t].posIdx + 1, triangles[t].uvIdx + 1, triangles[t].normIdx + 1,
            triangles[t+1].posIdx + 1, triangles[t+1].uvIdx + 1, triangles[t+1].normIdx + 1,
            triangles[t+2].posIdx + 1, triangles[t+2].uvIdx + 1, triangles[t+2].normIdx + 1);
    }

    fclose(f);
    printf("[INFO] Exported OBJ to %s\n", path);
}

// ----------------------------------------------------------------------
// Create the primordial world box (as a Brush)
// ----------------------------------------------------------------------
static Brush create_primordial_world_brush() {
    Brush worldBox;
    worldBox.type = BrushType::Add;
    worldBox.shape = ShapeType::Box;
    worldBox.center = {0, 0, 0};
    worldBox.size = {2000, 2000, 2000};
    for (auto& f : worldBox.faces) {
        f.texturePath = "void.jpg";
        f.scale = {1.0f, 1.0f};
        f.offset = {0.0f, 0.0f};
        f.rotation = 0.0f;
        f.worldLocked = false;
    }
    return worldBox;
}

// ----------------------------------------------------------------------
// Master CSG compiler using Manifold, with material indices
// ----------------------------------------------------------------------
std::vector<CSGPoly> compile_csg(
    const std::vector<Brush>& brushes,
    const std::vector<int>& materialIndices,
    const std::vector<VMISMaterial>& materials)   // now VMISMaterial
{
    std::vector<size_t> order(brushes.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(),
        [&](size_t a, size_t b) { return brushes[a].time < brushes[b].time; });

    manifold::Manifold current = brush_to_manifold(create_primordial_world_brush(), 0);

    int step = 1;
    for (size_t idx : order) {
        const Brush& brush = brushes[idx];
        int matIdx = materialIndices[idx];
        const char* typeStr = (brush.type == BrushType::Add) ? "Add" : "Sub";
        printf("[INFO] Processing brush %d: %s at time %d\n", step, typeStr, brush.time);

        manifold::Manifold brushManifold = brush_to_manifold(brush, matIdx);
        if (brush.type == BrushType::Add) {
            current = current.Boolean(brushManifold, manifold::OpType::Add);
        } else {
            current = current.Boolean(brushManifold, manifold::OpType::Subtract);
        }
        step++;
    }

    std::vector<CSGPoly> result = manifold_to_csgpolys(current, materials);
    printf("[INFO] CSG compilation complete: %zu polygons\n", result.size());
    return result;
}

// ----------------------------------------------------------------------
// Rebake .vmis: read brushes, run CSG, write back
// ----------------------------------------------------------------------
bool rebake_vmis(const char* vmisPath) {
    // 1. Read existing .vmis to get brush catalog
    std::vector<Brush> brushes;
    std::vector<Vertex> bakedVerts;
    std::vector<uint32_t> bakedIndices;
    std::vector<VMISMaterial> materials;
    std::vector<Triangle> physicsTris;
    std::vector<Vertex> debugLines;

    if (!read_vmis(vmisPath, brushes, bakedVerts, bakedIndices, materials, physicsTris, debugLines)) {
        fprintf(stderr, "[ERROR] Failed to read .vmis file: %s\n", vmisPath);
        return false;
    }

    if (brushes.empty()) {
        fprintf(stderr, "[ERROR] No brushes found in .vmis file.\n");
        return false;
    }

    printf("[INFO] Loaded %zu brushes from %s\n", brushes.size(), vmisPath);

    // 2. Build material table and indices from brushes (deduplicate)
    std::vector<VMISMaterial> matTable;
    std::unordered_map<std::string, int> texToIndex;
    std::vector<int> materialIndices;
    materialIndices.reserve(brushes.size());

    for (const auto& brush : brushes) {
        const FaceTexture& ft = brush.faces[0]; // all faces share same texture for now
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
            matIdx = (int)matTable.size();
            matTable.push_back(mat);
            texToIndex[key] = matIdx;
        } else {
            matIdx = it->second;
        }
        materialIndices.push_back(matIdx);
    }

    printf("[INFO] Built %zu materials from brushes\n", matTable.size());

    // 3. Run CSG
    std::vector<CSGPoly> polys = compile_csg(brushes, materialIndices, matTable);
    printf("[INFO] CSG produced %zu polygons\n", polys.size());

    // 4. Convert to VMIS format
    std::vector<Vertex> newVerts;
    std::vector<uint32_t> newIndices;
    for (const auto& poly : polys) {
        size_t n = poly.verts.size();
        if (n < 3) continue;
        int baseIdx = (int)newVerts.size();
        for (size_t i = 0; i < n; ++i) {
            Vertex v;
            v.x = poly.verts[i].pos.x;
            v.y = poly.verts[i].pos.y;
            v.z = poly.verts[i].pos.z;
            v.u = poly.verts[i].uv.x;
            v.v = poly.verts[i].uv.y;
            v.r = v.g = v.b = 1.0f;
            newVerts.push_back(v);
        }
        for (size_t i = 1; i < n - 1; ++i) {
            newIndices.push_back(baseIdx);
            newIndices.push_back(baseIdx + (int)i);
            newIndices.push_back(baseIdx + (int)i + 1);
        }
    }

    // Physics triangles (same as baked)
    std::vector<Triangle> newPhys;
    newPhys.reserve(newIndices.size() / 3);
    for (size_t i = 0; i < newIndices.size(); i += 3) {
        Triangle tri;
        tri.v0 = { newVerts[newIndices[i]].x,   newVerts[newIndices[i]].y,   newVerts[newIndices[i]].z };
        tri.v1 = { newVerts[newIndices[i+1]].x, newVerts[newIndices[i+1]].y, newVerts[newIndices[i+1]].z };
        tri.v2 = { newVerts[newIndices[i+2]].x, newVerts[newIndices[i+2]].y, newVerts[newIndices[i+2]].z };
        newPhys.push_back(tri);
    }

    // 5. Write back to the same .vmis file
    if (!write_vmis(vmisPath, brushes, newVerts, newIndices, matTable, newPhys, {})) {
        fprintf(stderr, "[ERROR] Failed to write .vmis file: %s\n", vmisPath);
        return false;
    }

    // 6. Optionally export OBJ for debugging
    std::string objPath = vmisPath;
    size_t dot = objPath.rfind('.');
    if (dot != std::string::npos && dot > 0) {
        objPath.replace(dot, std::string::npos, ".obj");
    } else {
        objPath += ".obj";
    }
    export_OBJ(polys, objPath.c_str());

    printf("[INFO] Rebaked .vmis: %s\n", vmisPath);
    return true;
}