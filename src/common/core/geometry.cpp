#include "geometry.h"
#include <cmath>
#include <algorithm>

static Vec2 apply_uv_transform(Vec2 uv, const FaceTexture& ft)
{
    // uv is already in texture space (world_units / scale)
    // Apply offset and rotation only
    uv.x += ft.offset.x;
    uv.y += ft.offset.y;
    float cosA = cosf(ft.rotation);
    float sinA = sinf(ft.rotation);
    float cx = 0.5f, cy = 0.5f;
    float dx = uv.x - cx;
    float dy = uv.y - cy;
    uv.x = cx + dx * cosA - dy * sinA;
    uv.y = cy + dx * sinA + dy * cosA;
    return uv;
}

static void get_char_uv(char c, float& u1, float& v1, float& u2, float& v2)
{
    int idx = (unsigned char)c - 32;
    if (idx < 0 || idx >= 95) idx = 0;
    int cols = 16, rows = 16;
    int grid_x = idx % cols;
    int grid_y = idx / cols;
    float tw = 1.0f / cols;
    float th = 1.0f / rows;
    u1 = grid_x * tw;
    v1 = grid_y * th;
    u2 = u1 + tw;
    v2 = v1 + th;
}

// ---- generate_box ----
void generate_box(const Vec3& center, const Vec3& size, const Vec3& color,
                  const FaceTexture faceData[NUM_FACES], bool invertWinding,
                  MeshData& outMainMesh, MeshData& outLabelMesh, bool showLabels)
{
    outMainMesh.vertices.clear();
    outMainMesh.indices.clear();
    outLabelMesh.vertices.clear();
    outLabelMesh.indices.clear();

    float hx = size.x * 0.5f;
    float hy = size.y * 0.5f;
    float hz = size.z * 0.5f;

    Vec3 corners[8] = {
        {-hx, -hy, -hz},
        { hx, -hy, -hz},
        { hx,  hy, -hz},
        {-hx,  hy, -hz},
        {-hx, -hy,  hz},
        { hx, -hy,  hz},
        { hx,  hy,  hz},
        {-hx,  hy,  hz}
    };

    int faceCorners[NUM_FACES][4] = {
        {4, 5, 6, 7}, // Front  (Z+)
        {0, 3, 2, 1}, // Back   (Z-)
        {0, 4, 7, 3}, // Left   (X-)
        {1, 2, 6, 5}, // Right  (X+)
        {3, 7, 6, 2}, // Top    (Y+)
        {0, 1, 5, 4}  // Bottom (Y-)
    };

    Vec3 faceNormals[NUM_FACES] = {
        {0,0,1}, {0,0,-1}, {-1,0,0}, {1,0,0}, {0,1,0}, {0,-1,0}
    };
    char faceLabels[NUM_FACES] = { 'F', 'B', 'L', 'R', 'T', 'D' };

    for (int f = 0; f < NUM_FACES; ++f) {
        const FaceTexture& ft = faceData[f];
        Vec2 rawUV[4];

        for (int i = 0; i < 4; ++i) {
            int idx = faceCorners[f][i];
            Vec3 localPos = corners[idx];
            Vec3 pos = ft.worldLocked ? (localPos + center) : localPos;
            float u, v;
            // Compute position along the face's 2D axes (in world units)
            switch (f) {
                case FACE_FRONT: // Z+ plane, axes: X (u), Y (v)
                    u = (pos.x - center.x + hx); // 0..size.x
                    v = (pos.y - center.y + hy);
                    break;
                case FACE_BACK: // Z- plane, axes: -X (u), Y (v)
                    u = (center.x + hx - pos.x);
                    v = (pos.y - center.y + hy);
                    break;
                case FACE_LEFT: // X- plane, axes: -Z (u), Y (v)
                    u = (center.z + hz - pos.z);
                    v = (pos.y - center.y + hy);
                    break;
                case FACE_RIGHT: // X+ plane, axes: Z (u), Y (v)
                    u = (pos.z - center.z + hz);
                    v = (pos.y - center.y + hy);
                    break;
                case FACE_TOP: // Y+ plane, axes: X (u), -Z (v)
                    u = (pos.x - center.x + hx);
                    v = (center.z + hz - pos.z);
                    break;
                case FACE_BOTTOM: // Y- plane, axes: X (u), Z (v)
                    u = (pos.x - center.x + hx);
                    v = (pos.z - center.z + hz);
                    break;
                default: u = v = 0.0f;
            }
            v = 1.0f - v;
            // Divide by scale to get UV coordinates (world units per repeat)
            u = u / ft.scale.x;
            v = v / ft.scale.y;
            rawUV[i] = { u, v };
        }

        // Apply offset and rotation
        Vec2 finalUV[4];
        for (int i = 0; i < 4; ++i)
            finalUV[i] = apply_uv_transform(rawUV[i], ft);

        int baseVertex = (int)outMainMesh.vertices.size();
        for (int i = 0; i < 4; ++i) {
            int idx = faceCorners[f][i];
            Vertex v;
            v.x = corners[idx].x + center.x;
            v.y = corners[idx].y + center.y;
            v.z = corners[idx].z + center.z;
            v.u = finalUV[i].x;
            v.v = finalUV[i].y;
            v.r = color.x;
            v.g = color.y;
            v.b = color.z;
            outMainMesh.vertices.push_back(v);
        }

        unsigned short base = (unsigned short)baseVertex;
        outMainMesh.indices.push_back(base + 0);
        outMainMesh.indices.push_back(base + 1);
        outMainMesh.indices.push_back(base + 2);
        outMainMesh.indices.push_back(base + 0);
        outMainMesh.indices.push_back(base + 2);
        outMainMesh.indices.push_back(base + 3);

        if (invertWinding) {
            std::swap(outMainMesh.indices[outMainMesh.indices.size() - 6],
                      outMainMesh.indices[outMainMesh.indices.size() - 4]);
            std::swap(outMainMesh.indices[outMainMesh.indices.size() - 3],
                      outMainMesh.indices[outMainMesh.indices.size() - 1]);
        }

        // ---- label ----
        if (showLabels) {
            Vec3 faceCenter = {0,0,0};
            for (int i = 0; i < 4; ++i) {
                int idx = faceCorners[f][i];
                faceCenter.x += corners[idx].x;
                faceCenter.y += corners[idx].y;
                faceCenter.z += corners[idx].z;
            }
            faceCenter.x = faceCenter.x / 4.0f + center.x;
            faceCenter.y = faceCenter.y / 4.0f + center.y;
            faceCenter.z = faceCenter.z / 4.0f + center.z;

            Vec3 normal = faceNormals[f];
            float labelSize = 0.4f;
            float labelHalf = labelSize * 0.5f;
            float offsetSign = invertWinding ? -1.0f : 1.0f;
            float offsetAmount = 0.05f;

            Vec3 up = {0,1,0};
            Vec3 right;
            if (fabsf(normal.y) > 0.9f) {
                right = {1,0,0};
                up = {0,0,1};
            } else {
                right = Vec3::cross(up, normal).normalized();
                up = Vec3::cross(normal, right).normalized();
            }

            Vec3 labelCorners[4];
            labelCorners[0] = faceCenter + (right * (-labelHalf)) + (up * (-labelHalf)) + normal * (offsetAmount * offsetSign);
            labelCorners[1] = faceCenter + (right * ( labelHalf)) + (up * (-labelHalf)) + normal * (offsetAmount * offsetSign);
            labelCorners[2] = faceCenter + (right * ( labelHalf)) + (up * ( labelHalf)) + normal * (offsetAmount * offsetSign);
            labelCorners[3] = faceCenter + (right * (-labelHalf)) + (up * ( labelHalf)) + normal * (offsetAmount * offsetSign);

            float u1, v1, u2, v2;
            get_char_uv(faceLabels[f], u1, v1, u2, v2);

            Vec2 labelUV[4];
            if (invertWinding) {
                labelUV[0] = {u1, v2};
                labelUV[1] = {u2, v2};
                labelUV[2] = {u2, v1};
                labelUV[3] = {u1, v1};
            } else {
                labelUV[0] = {u2, v2};
                labelUV[1] = {u1, v2};
                labelUV[2] = {u1, v1};
                labelUV[3] = {u2, v1};
            }

            int labelBase = (int)outLabelMesh.vertices.size();
            for (int i = 0; i < 4; ++i) {
                Vertex lv;
                lv.x = labelCorners[i].x;
                lv.y = labelCorners[i].y;
                lv.z = labelCorners[i].z;
                lv.u = labelUV[i].x;
                lv.v = labelUV[i].y;
                lv.r = 1.0f; lv.g = 1.0f; lv.b = 1.0f;
                outLabelMesh.vertices.push_back(lv);
            }
            unsigned short lBase = (unsigned short)labelBase;
            outLabelMesh.indices.push_back(lBase + 0);
            outLabelMesh.indices.push_back(lBase + 1);
            outLabelMesh.indices.push_back(lBase + 2);
            outLabelMesh.indices.push_back(lBase + 0);
            outLabelMesh.indices.push_back(lBase + 2);
            outLabelMesh.indices.push_back(lBase + 3);

            if (invertWinding) {
                std::swap(outLabelMesh.indices[outLabelMesh.indices.size() - 6],
                          outLabelMesh.indices[outLabelMesh.indices.size() - 4]);
                std::swap(outLabelMesh.indices[outLabelMesh.indices.size() - 3],
                          outLabelMesh.indices[outLabelMesh.indices.size() - 1]);
            }
        }
    }
}

// ---- generate_wedge ----
void generate_wedge(const Vec3& center, const Vec3& size, const Vec3& color,
                    const FaceTexture faceData[NUM_FACES], bool invertWinding,
                    MeshData& outMainMesh, MeshData& outLabelMesh, bool showLabels)
{
    outMainMesh.vertices.clear();
    outMainMesh.indices.clear();
    outLabelMesh.vertices.clear();
    outLabelMesh.indices.clear();

    float hx = size.x * 0.5f;
    float hy = size.y * 0.5f;
    float hz = size.z * 0.5f;

    Vec3 localPos[6] = {
        {-hx, -hy, -hz}, // 0: BLB
        { hx, -hy, -hz}, // 1: BRB
        {-hx, -hy,  hz}, // 2: BLF
        { hx, -hy,  hz}, // 3: BRF
        {-hx,  hy, -hz}, // 4: TLB
        { hx,  hy, -hz}  // 5: TRB
    };

    auto add_quad = [&](int a, int b, int c, int d, const Vec3& normal, const Vec3& uAxis, const Vec3& vAxis, const FaceTexture& ft) {
        int idx[4] = {a,b,c,d};
        int base = (int)outMainMesh.vertices.size();
        for (int i = 0; i < 4; ++i) {
            Vec3 local = localPos[idx[i]];
            Vec3 world = center + local;
            float u, v;
            if (ft.worldLocked) {
                u = (world.x * uAxis.x + world.y * uAxis.y + world.z * uAxis.z);
                v = (world.x * vAxis.x + world.y * vAxis.y + world.z * vAxis.z);
            } else {
                u = (local.x * uAxis.x + local.y * uAxis.y + local.z * uAxis.z);
                v = (local.x * vAxis.x + local.y * vAxis.y + local.z * vAxis.z);
            }
            v = 1.0f - v;
            u = u / ft.scale.x;
            v = v / ft.scale.y;
            Vec2 uv = apply_uv_transform({u, v}, ft);
            Vertex vert;
            vert.x = world.x; vert.y = world.y; vert.z = world.z;
            vert.u = uv.x; vert.v = uv.y;
            vert.r = color.x; vert.g = color.y; vert.b = color.z;
            outMainMesh.vertices.push_back(vert);
        }
        unsigned short baseShort = (unsigned short)base;
        outMainMesh.indices.push_back(baseShort + 0);
        outMainMesh.indices.push_back(baseShort + 1);
        outMainMesh.indices.push_back(baseShort + 2);
        outMainMesh.indices.push_back(baseShort + 0);
        outMainMesh.indices.push_back(baseShort + 2);
        outMainMesh.indices.push_back(baseShort + 3);
        if (invertWinding) {
            std::swap(outMainMesh.indices[outMainMesh.indices.size()-6], outMainMesh.indices[outMainMesh.indices.size()-4]);
            std::swap(outMainMesh.indices[outMainMesh.indices.size()-3], outMainMesh.indices[outMainMesh.indices.size()-1]);
        }
    };

    auto add_tri = [&](int a, int b, int c, const Vec3& normal, const Vec3& uAxis, const Vec3& vAxis, const FaceTexture& ft) {
        int idx[3] = {a,b,c};
        int base = (int)outMainMesh.vertices.size();
        for (int i = 0; i < 3; ++i) {
            Vec3 local = localPos[idx[i]];
            Vec3 world = center + local;
            float u, v;
            if (ft.worldLocked) {
                u = (world.x * uAxis.x + world.y * uAxis.y + world.z * uAxis.z);
                v = (world.x * vAxis.x + world.y * vAxis.y + world.z * vAxis.z);
            } else {
                u = (local.x * uAxis.x + local.y * uAxis.y + local.z * uAxis.z);
                v = (local.x * vAxis.x + local.y * vAxis.y + local.z * vAxis.z);
            }
            u = u / ft.scale.x;
            v = v / ft.scale.y;
            Vec2 uv = apply_uv_transform({u, v}, ft);
            Vertex vert;
            vert.x = world.x; vert.y = world.y; vert.z = world.z;
            vert.u = uv.x; vert.v = uv.y;
            vert.r = color.x; vert.g = color.y; vert.b = color.z;
            outMainMesh.vertices.push_back(vert);
        }
        unsigned short baseShort = (unsigned short)base;
        outMainMesh.indices.push_back(baseShort + 0);
        outMainMesh.indices.push_back(baseShort + 1);
        outMainMesh.indices.push_back(baseShort + 2);
        if (invertWinding) {
            std::swap(outMainMesh.indices[outMainMesh.indices.size()-3], outMainMesh.indices[outMainMesh.indices.size()-1]);
        }
    };

    const FaceTexture& ft = faceData[0];

    // ---- Main geometry ----
    add_quad(0,1,3,2, {0,-1,0}, {1,0,0}, {0,0,1}, ft); // bottom
    add_quad(4,5,1,0, {0,0,-1}, {1,0,0}, {0,1,0}, ft); // back
    Vec3 p2 = localPos[2], p3 = localPos[3], p5 = localPos[5];
    Vec3 e1 = p3 - p2;
    Vec3 e2 = p5 - p2;
    Vec3 normalSlope = Vec3::cross(e1, e2).normalized();
    add_quad(2,3,5,4, normalSlope, {1,0,0}, {0,1,0}, ft); // slope

    // ---- Left cap: clockwise when viewed from -X -> (2,0,4) ----
    add_tri(4,0,2, {-1,0,0}, {0,1,0}, {0,0,1}, ft);
    // ---- Right cap: clockwise when viewed from +X -> (3,1,5) ----
    add_tri(3,1,5, {1,0,0}, {0,1,0}, {0,0,1}, ft);

    // ---- Labels ----
    if (showLabels) {
        auto add_label_quad = [&](const std::vector<int>& indices, const Vec3& normal, const Vec3& uAxis, const Vec3& vAxis, char labelChar) {
            Vec3 faceCenter = {0,0,0};
            for (int idx : indices)
                faceCenter = faceCenter + localPos[idx];
            faceCenter = faceCenter * (1.0f / (float)indices.size());
            faceCenter = center + faceCenter;

            float labelSize = 0.4f;
            float labelHalf = labelSize * 0.5f;
            float offsetSign = invertWinding ? -1.0f : 1.0f;
            float offsetAmount = 0.05f;

            Vec3 up = {0,1,0};
            Vec3 right;
            if (fabsf(normal.y) > 0.9f) {
                right = {1,0,0};
                up = {0,0,1};
            } else {
                right = Vec3::cross(up, normal).normalized();
                up = Vec3::cross(normal, right).normalized();
            }

            Vec3 labelCorners[4];
            labelCorners[0] = faceCenter + (right * (-labelHalf)) + (up * (-labelHalf)) + normal * (offsetAmount * offsetSign);
            labelCorners[1] = faceCenter + (right * ( labelHalf)) + (up * (-labelHalf)) + normal * (offsetAmount * offsetSign);
            labelCorners[2] = faceCenter + (right * ( labelHalf)) + (up * ( labelHalf)) + normal * (offsetAmount * offsetSign);
            labelCorners[3] = faceCenter + (right * (-labelHalf)) + (up * ( labelHalf)) + normal * (offsetAmount * offsetSign);

            float u1, v1, u2, v2;
            get_char_uv(labelChar, u1, v1, u2, v2);
            Vec2 labelUV[4];
            if (invertWinding) {
                labelUV[0] = {u1, v2};
                labelUV[1] = {u2, v2};
                labelUV[2] = {u2, v1};
                labelUV[3] = {u1, v1};
            } else {
                labelUV[0] = {u2, v2};
                labelUV[1] = {u1, v2};
                labelUV[2] = {u1, v1};
                labelUV[3] = {u2, v1};
            }

            int labelBase = (int)outLabelMesh.vertices.size();
            for (int i = 0; i < 4; ++i) {
                Vertex lv;
                lv.x = labelCorners[i].x;
                lv.y = labelCorners[i].y;
                lv.z = labelCorners[i].z;
                lv.u = labelUV[i].x;
                lv.v = labelUV[i].y;
                lv.r = 1.0f; lv.g = 1.0f; lv.b = 1.0f;
                outLabelMesh.vertices.push_back(lv);
            }
            unsigned short lBase = (unsigned short)labelBase;
            outLabelMesh.indices.push_back(lBase + 0);
            outLabelMesh.indices.push_back(lBase + 1);
            outLabelMesh.indices.push_back(lBase + 2);
            outLabelMesh.indices.push_back(lBase + 0);
            outLabelMesh.indices.push_back(lBase + 2);
            outLabelMesh.indices.push_back(lBase + 3);
            if (invertWinding) {
                std::swap(outLabelMesh.indices[outLabelMesh.indices.size()-6], outLabelMesh.indices[outLabelMesh.indices.size()-4]);
                std::swap(outLabelMesh.indices[outLabelMesh.indices.size()-3], outLabelMesh.indices[outLabelMesh.indices.size()-1]);
            }
        };

        auto add_label_tri = [&](const std::vector<int>& indices, const Vec3& normal, const Vec3& uAxis, const Vec3& vAxis, char labelChar) {
            add_label_quad(indices, normal, uAxis, vAxis, labelChar);
        };

        add_label_quad({0,1,3,2}, {0,-1,0}, {1,0,0}, {0,0,1}, 'D');
        add_label_quad({0,1,5,4}, {0,0,-1}, {1,0,0}, {0,1,0}, 'B');
        add_label_quad({2,3,5,4}, normalSlope, {1,0,0}, {0,1,0}, 'S');
        add_label_tri({2,0,4}, {-1,0,0}, {0,1,0}, {0,0,1}, 'L');
        add_label_tri({3,1,5}, {1,0,0}, {0,1,0}, {0,0,1}, 'R');
    }
}


void generate_box_wireframe(const Vec3& center, const Vec3& size, const Vec3& color,
                            std::vector<Vertex>& outLineVerts) {
    float hx = size.x * 0.5f;
    float hy = size.y * 0.5f;
    float hz = size.z * 0.5f;

    Vec3 corners[8] = {
        {-hx, -hy, -hz}, { hx, -hy, -hz}, { hx,  hy, -hz}, {-hx,  hy, -hz},
        {-hx, -hy,  hz}, { hx, -hy,  hz}, { hx,  hy,  hz}, {-hx,  hy,  hz}
    };

    // Add center offset
    for (auto& c : corners) {
        c = c + center;
    }

    auto add_line = [&](const Vec3& a, const Vec3& b) {
        Vertex v1{a.x,a.y,a.z, 0,0, color.x,color.y,color.z};
        Vertex v2{b.x,b.y,b.z, 0,0, color.x,color.y,color.z};
        outLineVerts.push_back(v1);
        outLineVerts.push_back(v2);
    };

    // Bottom face
    add_line(corners[0], corners[1]);
    add_line(corners[1], corners[2]);
    add_line(corners[2], corners[3]);
    add_line(corners[3], corners[0]);

    // Top face
    add_line(corners[4], corners[5]);
    add_line(corners[5], corners[6]);
    add_line(corners[6], corners[7]);
    add_line(corners[7], corners[4]);

    // Vertical edges
    add_line(corners[0], corners[4]);
    add_line(corners[1], corners[5]);
    add_line(corners[2], corners[6]);
    add_line(corners[3], corners[7]);
}

void generate_wedge_wireframe(const Vec3& center, const Vec3& size, const Vec3& color,
                              std::vector<Vertex>& outLineVerts) {
    float hx = size.x * 0.5f;
    float hy = size.y * 0.5f;
    float hz = size.z * 0.5f;

    // Wedge local vertices (matching geometry.cpp order):
    // 0: BLB, 1: BRB, 2: BLF, 3: BRF, 4: TLB, 5: TRB
    Vec3 local[6] = {
        {-hx, -hy, -hz}, // 0
        { hx, -hy, -hz}, // 1
        {-hx, -hy,  hz}, // 2
        { hx, -hy,  hz}, // 3
        {-hx,  hy, -hz}, // 4
        { hx,  hy, -hz}  // 5
    };
    for (auto& v : local) v = v + center;

    auto add_line = [&](const Vec3& a, const Vec3& b) {
        Vertex v1{a.x,a.y,a.z, 0,0, color.x,color.y,color.z};
        Vertex v2{b.x,b.y,b.z, 0,0, color.x,color.y,color.z};
        outLineVerts.push_back(v1);
        outLineVerts.push_back(v2);
    };

    // Bottom face (0,1,3,2)
    add_line(local[0], local[1]);
    add_line(local[1], local[3]);
    add_line(local[3], local[2]);
    add_line(local[2], local[0]);

    // Back face (0,1,5,4)
    add_line(local[0], local[1]);
    add_line(local[1], local[5]);
    add_line(local[5], local[4]);
    add_line(local[4], local[0]);

    // Slope face (2,3,5,4)
    add_line(local[2], local[3]);
    add_line(local[3], local[5]);
    add_line(local[5], local[4]);
    add_line(local[4], local[2]);

    // Left cap (2,0,4) – triangle edges
    add_line(local[2], local[0]);
    add_line(local[0], local[4]);
    add_line(local[4], local[2]);

    // Right cap (3,1,5)
    add_line(local[3], local[1]);
    add_line(local[1], local[5]);
    add_line(local[5], local[3]);
}
