#pragma once
#include "core/math.h"
#include <vector>
#include <cfloat>

struct Triangle
{
    Vec3 v0, v1, v2;
};

struct HitResult
{
    bool hit = false;
    Vec3 normal = {0.0f, 1.0f, 0.0f};
    float penetration = 0.0f;
};

// ---- AABB for BVH ----
struct AABB
{
    Vec3 min;
    Vec3 max;

    bool overlaps(const AABB& other) const {
        return (min.x <= other.max.x && max.x >= other.min.x) &&
               (min.y <= other.max.y && max.y >= other.min.y) &&
               (min.z <= other.max.z && max.z >= other.min.z);
    }

    static AABB from_triangle(const Triangle& tri) {
        AABB box;
        box.min.x = fminf(tri.v0.x, fminf(tri.v1.x, tri.v2.x));
        box.min.y = fminf(tri.v0.y, fminf(tri.v1.y, tri.v2.y));
        box.min.z = fminf(tri.v0.z, fminf(tri.v1.z, tri.v2.z));
        box.max.x = fmaxf(tri.v0.x, fmaxf(tri.v1.x, tri.v2.x));
        box.max.y = fmaxf(tri.v0.y, fmaxf(tri.v1.y, tri.v2.y));
        box.max.z = fmaxf(tri.v0.z, fmaxf(tri.v1.z, tri.v2.z));
        return box;
    }

    static AABB from_capsule(const Vec3& center, float radius, float halfHeight) {
        AABB box;
        box.min.x = center.x - radius;
        box.max.x = center.x + radius;
        box.min.z = center.z - radius;
        box.max.z = center.z + radius;
        box.min.y = center.y - halfHeight - radius;
        box.max.y = center.y + halfHeight + radius;
        return box;
    }
};

class PhysicsWorld
{
public:
    void build(const std::vector<Triangle>& triangles);
    bool capsule_hit(const Vec3& center, float radius, float halfHeight, HitResult& outHit) const;

    const std::vector<Triangle>& get_triangles() const { return m_triangles; }

private:
    // ---- BVH node ----
    struct BVHNode
    {
        AABB bounds;
        int leftChild = -1;
        int rightChild = -1;
        int firstTriIndex = -1;   // leaf: start index in m_triIndices
        int triCount = 0;         // leaf: number of triangles
    };

    // ---- Build helpers ----
    int build_node(std::vector<int>& triIndices, int start, int count, int depth);
    void query_node(int nodeIdx, const AABB& box, std::vector<int>& outIndices) const;

    // ---- Data ----
    std::vector<Triangle> m_triangles;
    std::vector<int> m_triIndices;           // reordered for BVH leaves
    std::vector<BVHNode> m_nodes;
    int m_root = -1;

    static constexpr int LEAF_SIZE = 4;
};