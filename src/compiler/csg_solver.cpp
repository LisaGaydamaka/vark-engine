#undef min
#undef max

#include "csg_solver.h"
#include "core/geometry.h"
#include "world/vmb_format.h"
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
//  Convert a brush to manifold::MeshGL, embedding UVs and material index
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

    // Set faceID per triangle to the material index
    gl.faceID.resize(mesh.indices.size() / 3, static_cast<uint32_t>(materialIndex));

    gl.Merge(); // optional, helps manifoldness
    return gl;
}

// ----------------------------------------------------------------------
//  Convert a brush to manifold::Manifold
// ----------------------------------------------------------------------

static manifold::Manifold brush_to_manifold(const Brush& brush, int materialIndex) {
    manifold::MeshGL gl = brush_to_meshgl(brush, materialIndex);
    manifold::Manifold m(gl);
    if (m.Status() != manifold::Manifold::Error::NoError) {
        printf("[WARN] Brush manifold status %d\n", (int)m.Status());
    }
    return m;
}

// ----------------------------------------------------------------------
//  Convert manifold::Manifold to CSGPoly list, reading UVs and material index
// ----------------------------------------------------------------------

static std::vector<CSGPoly> manifold_to_csgpolys(
    const manifold::Manifold& m,
    const std::vector<VMBMaterial>& materials)
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
        // Compute normal
        Vec3 e1 = poly.verts[1].pos - poly.verts[0].pos;
        Vec3 e2 = poly.verts[2].pos - poly.verts[0].pos;
        poly.normal = Vec3::cross(e1, e2).normalized();

        // Retrieve material from faceID
        uint32_t matIdx = gl.faceID[t]; // faceID per triangle
        if (matIdx < materials.size()) {
            const auto& mat = materials[matIdx];
            poly.texture.texturePath = mat.textureName;
            poly.texture.scale = {mat.uScale, mat.vScale};
            poly.texture.offset = {mat.offsetX, mat.offsetY};
            poly.texture.rotation = mat.rotation;
            poly.worldLocked = mat.worldLocked != 0;
        } else {
            // fallback
            poly.texture.texturePath = "zebra.png";
            poly.texture.scale = {16.0f, 16.0f};
            poly.worldLocked = false;
        }
        polys.push_back(poly);
    }
    return polys;
}

// ----------------------------------------------------------------------
//  Save VMB (now uses the pre-built material list)
// ----------------------------------------------------------------------

bool save_vmb(const char* path, const std::vector<CSGPoly>& polys) {
    std::vector<VMBVertex> verts;
    std::vector<VMBTriangle> tris;
    std::vector<VMBMaterial> materials;
    std::unordered_map<std::string, int> texToIndex;

    for (const auto& poly : polys) {
        if (poly.verts.size() < 3) continue;
        int baseIdx = (int)verts.size();
        for (size_t i = 0; i < poly.verts.size(); ++i) {
            VMBVertex v;
            v.x = poly.verts[i].pos.x;
            v.y = poly.verts[i].pos.y;
            v.z = poly.verts[i].pos.z;
            v.u = poly.verts[i].uv.x;
            v.v = poly.verts[i].uv.y;
            verts.push_back(v);
        }
        for (size_t i = 1; i < poly.verts.size() - 1; ++i) {
            VMBTriangle tri;
            tri.i0 = baseIdx;
            tri.i1 = baseIdx + (int)i;
            tri.i2 = baseIdx + (int)i + 1;
            const std::string& texPath = poly.texture.texturePath;
            auto it = texToIndex.find(texPath);
            if (it == texToIndex.end()) {
                VMBMaterial mat;
                strncpy_s(mat.textureName, sizeof(mat.textureName), texPath.c_str(), _TRUNCATE);
                mat.textureName[63] = '\0';
                mat.uScale = poly.texture.scale.x;
                mat.vScale = poly.texture.scale.y;
                mat.rotation = poly.texture.rotation;
                mat.offsetX = poly.texture.offset.x;
                mat.offsetY = poly.texture.offset.y;
                mat.worldLocked = poly.worldLocked ? 1 : 0;
                int idx = (int)materials.size();
                materials.push_back(mat);
                texToIndex[texPath] = idx;
                tri.materialIndex = idx;
            } else {
                tri.materialIndex = it->second;
            }
            tris.push_back(tri);
        }
    }

    FILE* f = nullptr;
    fopen_s(&f, path, "wb");
    if (!f) return false;

    VMBHeader header;
    memcpy(header.magic, "VMB ", 4);
    header.version = 1;
    header.vertexCount = (uint32_t)verts.size();
    header.indexCount = (uint32_t)tris.size() * 3;
    header.materialCount = (uint32_t)materials.size();
    header.physicsTriCount = (uint32_t)tris.size();

    fwrite(&header, sizeof(header), 1, f);
    fwrite(verts.data(), sizeof(VMBVertex), verts.size(), f);
    fwrite(tris.data(), sizeof(VMBTriangle), tris.size(), f);
    fwrite(materials.data(), sizeof(VMBMaterial), materials.size(), f);

    fclose(f);
    printf("[INFO] Saved %zu vertices, %zu triangles, %zu materials to %s\n",
           verts.size(), tris.size(), materials.size(), path);
    return true;
}

// ----------------------------------------------------------------------
// Export to OBJ (wavefront)
// ----------------------------------------------------------------------
void export_OBJ(const std::vector<CSGPoly>& polys, const char* path)
{
    FILE* f = nullptr;
    fopen_s(&f, path, "w");
    if (!f) {
        fprintf(stderr, "[ERROR] Failed to open OBJ file for writing: %s\n", path);
        return;
    }

    fprintf(f, "# Exported from Vibe Engine CSG\n");
    fprintf(f, "o CSGResult\n");

    std::vector<Vec3> positions;
    std::vector<Vec2> uvs;
    std::vector<Vec3> normals;

    struct TriIndices { int posIdx, uvIdx, normIdx; };
    std::vector<TriIndices> triangles;
    std::vector<Vec3> faceNormals;

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

    for (const auto& p : positions) {
        fprintf(f, "v %f %f %f\n", p.x, p.y, p.z);
    }
    for (const auto& uv : uvs) {
        fprintf(f, "vt %f %f\n", uv.x, uv.y);
    }
    for (const auto& n : normals) {
        fprintf(f, "vn %f %f %f\n", n.x, n.y, n.z);
    }

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
//  .vm file parser
// ----------------------------------------------------------------------

static bool load_vm_file(const char* filepath, std::vector<Brush>& outBrushes) {
    outBrushes.clear();

    std::ifstream file(filepath);
    if (!file.is_open()) {
        fprintf(stderr, "[ERROR] Failed to open level file: %s\n", filepath);
        return false;
    }

    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line)) {
        lineNumber++;
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        if (line[start] == '#') continue;

        std::istringstream iss(line);
        std::string typeStr, shapeStr;
        float x, y, z, w, h, d;
        std::string texture = "zebra.png";
        float uScale = 16.0f, vScale = 16.0f;
        int time = 0;

        if (!(iss >> typeStr >> shapeStr >> x >> y >> z >> w >> h >> d)) {
            printf("[WARN] Invalid line %d\n", lineNumber);
            continue;
        }

        BrushType brushType;
        if (typeStr == "add") brushType = BrushType::Add;
        else if (typeStr == "sub") brushType = BrushType::Sub;
        else {
            printf("[WARN] Invalid type '%s' at line %d\n", typeStr.c_str(), lineNumber);
            continue;
        }

        ShapeType shapeType;
        if (shapeStr == "box") shapeType = ShapeType::Box;
        else if (shapeStr == "wedge") shapeType = ShapeType::Wedge;
        else {
            printf("[WARN] Invalid shape '%s' at line %d\n", shapeStr.c_str(), lineNumber);
            continue;
        }

        if (!(iss >> texture)) texture = "zebra.png";
        if (!(iss >> uScale)) uScale = 16.0f;
        if (!(iss >> vScale)) vScale = 16.0f;
        if (!(iss >> time)) time = 0;

        Brush brush;
        brush.center = { x, y, z };
        brush.size = { w, h, d };
        brush.type = brushType;
        brush.shape = shapeType;
        brush.color = { 1.0f, 1.0f, 1.0f };
        brush.time = time;

        for (int i = 0; i < NUM_FACES; ++i) {
            brush.faces[i].texturePath = texture;
            brush.faces[i].offset = { 0.0f, 0.0f };
            brush.faces[i].scale = { uScale, vScale };
            brush.faces[i].rotation = 0.0f;
            brush.faces[i].worldLocked = false;
        }

        outBrushes.push_back(brush);
    }

    file.close();
    printf("[INFO] Loaded %zu brushes from %s\n", outBrushes.size(), filepath);
    return true;
}

// ----------------------------------------------------------------------
//  Create the primordial world box (as a Brush)
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
//  Master CSG compiler using Manifold, with material indices
// ----------------------------------------------------------------------

std::vector<CSGPoly> compile_csg(
    const std::vector<Brush>& brushes,
    const std::vector<int>& materialIndices,
    const std::vector<VMBMaterial>& materials)
{
    // Sort by time (lowest first), keeping material indices in sync
    std::vector<size_t> order(brushes.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(),
        [&](size_t a, size_t b) { return brushes[a].time < brushes[b].time; });

    // Start with the finite solid world box
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
//  Compile .vm → .vmb (entry point)
// ----------------------------------------------------------------------

bool compile_vm_to_vmb(const char* vmPath, const char* vmbPath) {
    std::vector<Brush> brushes;
    if (!load_vm_file(vmPath, brushes)) {
        fprintf(stderr, "[ERROR] Failed to load .vm file: %s\n", vmPath);
        return false;
    }

    if (brushes.empty()) {
        fprintf(stderr, "[ERROR] No brushes found in .vm file.\n");
        return false;
    }

    // Build material list and assign indices
    std::vector<VMBMaterial> materials;
    std::unordered_map<std::string, int> texToIndex;
    std::vector<int> materialIndices;
    materialIndices.reserve(brushes.size());

    for (const auto& brush : brushes) {
        const FaceTexture& ft = brush.faces[0];
        std::string key = ft.texturePath + "|" + std::to_string(ft.scale.x) + "|" + std::to_string(ft.scale.y);
        auto it = texToIndex.find(key);
        int matIdx;
        if (it == texToIndex.end()) {
            VMBMaterial mat;
            strncpy_s(mat.textureName, sizeof(mat.textureName), ft.texturePath.c_str(), _TRUNCATE);
            mat.textureName[63] = '\0';
            mat.uScale = ft.scale.x;
            mat.vScale = ft.scale.y;
            mat.rotation = ft.rotation;
            mat.offsetX = ft.offset.x;
            mat.offsetY = ft.offset.y;
            mat.worldLocked = ft.worldLocked ? 1 : 0;
            matIdx = (int)materials.size();
            materials.push_back(mat);
            texToIndex[key] = matIdx;
        } else {
            matIdx = it->second;
        }
        materialIndices.push_back(matIdx);
    }

    printf("[INFO] Loaded %zu brushes, %zu unique materials\n", brushes.size(), materials.size());
    printf("[INFO] Compiling CSG with Manifold...\n");

    std::vector<CSGPoly> result = compile_csg(brushes, materialIndices, materials);
    printf("[INFO] CSG compilation complete: %zu polygons\n", result.size());

    if (!save_vmb(vmbPath, result)) {
        fprintf(stderr, "[ERROR] Failed to save .vmb file: %s\n", vmbPath);
        return false;
    }

    // Export OBJ
    std::string objPath = vmbPath;
    size_t dot = objPath.rfind('.');
    if (dot != std::string::npos && dot > 0) {
        objPath.replace(dot, std::string::npos, ".obj");
    } else {
        objPath += ".obj";
    }
    export_OBJ(result, objPath.c_str());

    printf("[INFO] Successfully saved %s and %s\n", vmbPath, objPath.c_str());
    return true;
}

// ----------------------------------------------------------------------
//  Compile .vm → .vmis
// ----------------------------------------------------------------------

bool compile_vm_to_vmis(const char* vmPath, const char* vmisPath)
{
    // 1. Load brushes
    std::vector<Brush> brushes;
    if (!load_vm_file(vmPath, brushes)) {
        fprintf(stderr, "[ERROR] Failed to load .vm file: %s\n", vmPath);
        return false;
    }
    if (brushes.empty()) {
        fprintf(stderr, "[ERROR] No brushes found in .vm file.\n");
        return false;
    }

    // 2. Build material table (VMBMaterial) and per‑brush indices
    std::vector<VMBMaterial> materials;
    std::unordered_map<std::string, int> texToIndex;
    std::vector<int> materialIndices;
    materialIndices.reserve(brushes.size());

    for (const auto& brush : brushes) {
        const FaceTexture& ft = brush.faces[0];
        std::string key = ft.texturePath + "|" + std::to_string(ft.scale.x) + "|" + std::to_string(ft.scale.y);
        auto it = texToIndex.find(key);
        int matIdx;
        if (it == texToIndex.end()) {
            VMBMaterial mat;
            strncpy_s(mat.textureName, sizeof(mat.textureName), ft.texturePath.c_str(), _TRUNCATE);
            mat.textureName[63] = '\0';
            mat.uScale = ft.scale.x;
            mat.vScale = ft.scale.y;
            mat.rotation = ft.rotation;
            mat.offsetX = ft.offset.x;
            mat.offsetY = ft.offset.y;
            mat.worldLocked = ft.worldLocked ? 1 : 0;
            matIdx = (int)materials.size();
            materials.push_back(mat);
            texToIndex[key] = matIdx;
        } else {
            matIdx = it->second;
        }
        materialIndices.push_back(matIdx);
    }

    printf("[INFO] Loaded %zu brushes, %zu unique materials\n", brushes.size(), materials.size());

    // 3. Run CSG
    std::vector<CSGPoly> polys = compile_csg(brushes, materialIndices, materials);
    printf("[INFO] CSG produced %zu polygons\n", polys.size());

    // 4. Convert CSGPoly to Vertex + indices (triangulate each polygon)
    std::vector<Vertex> bakedVerts;
    std::vector<uint32_t> bakedIndices;

    for (const auto& poly : polys) {
        size_t n = poly.verts.size();
        if (n < 3) continue;
        int baseIdx = (int)bakedVerts.size();
        for (size_t i = 0; i < n; ++i) {
            Vertex v;
            v.x = poly.verts[i].pos.x;
            v.y = poly.verts[i].pos.y;
            v.z = poly.verts[i].pos.z;
            v.u = poly.verts[i].uv.x;
            v.v = poly.verts[i].uv.y;
            v.r = 1.0f; v.g = 1.0f; v.b = 1.0f;
            bakedVerts.push_back(v);
        }
        // Fan triangulation: 0, i, i+1
        for (size_t i = 1; i < n - 1; ++i) {
            bakedIndices.push_back(baseIdx);
            bakedIndices.push_back(baseIdx + (int)i);
            bakedIndices.push_back(baseIdx + (int)i + 1);
        }
    }

    // 5. Convert VMBMaterial → VMISMaterial
    std::vector<VMISMaterial> vmisMats;
    vmisMats.reserve(materials.size());
    for (const auto& mb : materials) {
        VMISMaterial vm;
        strncpy_s(vm.texturePath, sizeof(vm.texturePath), mb.textureName, _TRUNCATE);
        vm.texturePath[63] = '\0';
        vm.scaleX = mb.uScale;
        vm.scaleY = mb.vScale;
        vm.offsetX = mb.offsetX;
        vm.offsetY = mb.offsetY;
        vm.rotation = mb.rotation;
        vm.worldLocked = mb.worldLocked;
        vmisMats.push_back(vm);
    }

    // 6. Physics mesh: use the same triangles as baked
    std::vector<Triangle> physicsTris;
    physicsTris.reserve(bakedIndices.size() / 3);
    for (size_t i = 0; i < bakedIndices.size(); i += 3) {
        Triangle tri;
        tri.v0 = { bakedVerts[bakedIndices[i]].x,     bakedVerts[bakedIndices[i]].y,     bakedVerts[bakedIndices[i]].z };
        tri.v1 = { bakedVerts[bakedIndices[i+1]].x,   bakedVerts[bakedIndices[i+1]].y,   bakedVerts[bakedIndices[i+1]].z };
        tri.v2 = { bakedVerts[bakedIndices[i+2]].x,   bakedVerts[bakedIndices[i+2]].y,   bakedVerts[bakedIndices[i+2]].z };
        physicsTris.push_back(tri);
    }

    // 7. Debug wireframe: empty (optional)
    std::vector<Vertex> debugLines;

    // 8. Write VMIS
    if (!write_vmis(vmisPath, brushes, bakedVerts, bakedIndices, vmisMats, physicsTris, debugLines)) {
        fprintf(stderr, "[ERROR] Failed to write .vmis file: %s\n", vmisPath);
        return false;
    }

    // 9. Verify by reading back
    std::vector<Brush> loadedBrushes;
    std::vector<Vertex> loadedVerts;
    std::vector<uint32_t> loadedIndices;
    std::vector<VMISMaterial> loadedMats;
    std::vector<Triangle> loadedPhys;
    std::vector<Vertex> loadedDebug;

    if (read_vmis(vmisPath, loadedBrushes, loadedVerts, loadedIndices, loadedMats, loadedPhys, loadedDebug)) {
        printf("[INFO] Read-back successful: %zu brushes, %zu vertices, %zu indices, %zu materials, %zu phys tris\n",
            loadedBrushes.size(), loadedVerts.size(), loadedIndices.size(), loadedMats.size(), loadedPhys.size());
    } else {
        fprintf(stderr, "[ERROR] Read-back failed!\n");
    }

    printf("[INFO] Successfully compiled to .vmis: %s\n", vmisPath);
    return true;
}