#include "physics_world.h"
#include <cmath>
#include <algorithm>
#include <cfloat>

// ---- Local helpers ----

static inline float clampf(float v, float min, float max) {
    return (v < min) ? min : (v > max) ? max : v;
}

static float length_sq(const Vec3& v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

static Vec3 closest_point_on_segment(const Vec3& p, const Vec3& a, const Vec3& b) {
    Vec3 ab = b - a;
    float t = Vec3::dot(p - a, ab) / Vec3::dot(ab, ab);
    t = clampf(t, 0.0f, 1.0f);
    return a + ab * t;
}

static bool point_in_triangle(const Vec3& p, const Vec3& a, const Vec3& b, const Vec3& c) {
    Vec3 v0 = c - a;
    Vec3 v1 = b - a;
    Vec3 v2 = p - a;
    float dot00 = Vec3::dot(v0, v0);
    float dot01 = Vec3::dot(v0, v1);
    float dot02 = Vec3::dot(v0, v2);
    float dot11 = Vec3::dot(v1, v1);
    float dot12 = Vec3::dot(v1, v2);
    float invDenom = 1.0f / (dot00 * dot11 - dot01 * dot01);
    float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
    float v = (dot00 * dot12 - dot01 * dot02) * invDenom;
    return (u >= 0.0f) && (v >= 0.0f) && (u + v <= 1.0f);
}

static void closest_points_segments(const Vec3& p1, const Vec3& q1,
                                    const Vec3& p2, const Vec3& q2,
                                    Vec3& outP, Vec3& outQ, float& outDistSq) {
    Vec3 d1 = q1 - p1;
    Vec3 d2 = q2 - p2;
    Vec3 r = p1 - p2;
    float a = Vec3::dot(d1, d1);
    float b = Vec3::dot(d1, d2);
    float c = Vec3::dot(d1, r);
    float e = Vec3::dot(d2, d2);
    float f = Vec3::dot(d2, r);
    float denom = a * e - b * b;
    float s, t;
    if (denom != 0.0f) {
        s = (b * f - c * e) / denom;
        t = (a * f - b * c) / denom;
    } else {
        s = 0.0f;
        t = f / e;
    }
    s = clampf(s, 0.0f, 1.0f);
    t = clampf(t, 0.0f, 1.0f);
    outP = p1 + d1 * s;
    outQ = p2 + d2 * t;
    Vec3 diff = outP - outQ;
    outDistSq = Vec3::dot(diff, diff);
}

static Vec3 closest_point_on_triangle(const Vec3& p, const Vec3& a, const Vec3& b, const Vec3& c) {
    Vec3 ab = b - a;
    Vec3 ac = c - a;
    Vec3 ap = p - a;

    float d1 = Vec3::dot(ab, ap);
    float d2 = Vec3::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return a;

    Vec3 bp = p - b;
    float d3 = Vec3::dot(ab, bp);
    float d4 = Vec3::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return b;

    Vec3 cp = p - c;
    float d5 = Vec3::dot(ab, cp);
    float d6 = Vec3::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return c;

    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        float v = d1 / (d1 - d3);
        return a + ab * v;
    }

    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        float w = d2 / (d2 - d6);
        return a + ac * w;
    }

    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + (c - b) * w;
    }

    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    return a + ab * v + ac * w;
}

// ---- Exact distance between a segment and a triangle ----
static float distance_segment_triangle(const Vec3& A, const Vec3& B,
                                       const Vec3& T0, const Vec3& T1, const Vec3& T2,
                                       Vec3& segPoint, Vec3& triPoint) {
    float minDistSq = FLT_MAX;
    Vec3 bestSeg = A;
    Vec3 bestTri = T0;

    // 1. Closest point on triangle to A, then clamp to segment
    Vec3 pA = closest_point_on_triangle(A, T0, T1, T2);
    Vec3 sA = closest_point_on_segment(pA, A, B);
    float dA = length_sq(pA - sA);
    if (dA < minDistSq) { minDistSq = dA; bestSeg = sA; bestTri = pA; }

    // 2. Closest point on triangle to B
    Vec3 pB = closest_point_on_triangle(B, T0, T1, T2);
    Vec3 sB = closest_point_on_segment(pB, A, B);
    float dB = length_sq(pB - sB);
    if (dB < minDistSq) { minDistSq = dB; bestSeg = sB; bestTri = pB; }

    // 3. For each triangle vertex, closest point on segment
    Vec3 verts[3] = {T0, T1, T2};
    for (int i = 0; i < 3; ++i) {
        Vec3 s = closest_point_on_segment(verts[i], A, B);
        float d = length_sq(verts[i] - s);
        if (d < minDistSq) { minDistSq = d; bestSeg = s; bestTri = verts[i]; }
    }

    // 4. For each triangle edge, segment–segment closest points
    Vec3 edges[3][2] = {{T0, T1}, {T1, T2}, {T2, T0}};
    for (int i = 0; i < 3; ++i) {
        Vec3 P, Q;
        float dSq;
        closest_points_segments(A, B, edges[i][0], edges[i][1], P, Q, dSq);
        if (dSq < minDistSq) { minDistSq = dSq; bestSeg = P; bestTri = Q; }
    }

    // 5. Triangle interior: distance from segment to plane, check if projection is inside
    Vec3 normal = Vec3::cross(T1 - T0, T2 - T0).normalized();
    float d = Vec3::dot(normal, T0);
    float da = Vec3::dot(normal, A) - d;
    float db = Vec3::dot(normal, B) - d;

    // If segment intersects plane
    if (da * db < 0.0f || fabsf(da) < 1e-6f || fabsf(db) < 1e-6f) {
        float t = -da / (db - da);
        t = clampf(t, 0.0f, 1.0f);
        Vec3 P = A + (B - A) * t;
        if (point_in_triangle(P, T0, T1, T2)) {
            minDistSq = 0.0f;
            bestSeg = P;
            bestTri = P;
        }
    } else {
        // Segment does not intersect plane; closest point on segment to plane is endpoint with smaller abs distance
        Vec3 Q = (fabsf(da) < fabsf(db)) ? A : B;
        Vec3 Qproj = Q - normal * (Vec3::dot(normal, Q) - d);
        if (point_in_triangle(Qproj, T0, T1, T2)) {
            float dist = fabsf(Vec3::dot(normal, Q) - d);
            float dSq = dist * dist;
            if (dSq < minDistSq) {
                minDistSq = dSq;
                bestSeg = Q;
                bestTri = Qproj;
            }
        }
    }

    segPoint = bestSeg;
    triPoint = bestTri;
    return sqrtf(minDistSq);
}

// ---- BVH Implementation ----

int PhysicsWorld::build_node(std::vector<int>& triIndices, int start, int count, int depth) {
    (void)depth;

    // Compute bounds of this node
    AABB bounds;
    bounds.min = {FLT_MAX, FLT_MAX, FLT_MAX};
    bounds.max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

    for (int i = start; i < start + count; ++i) {
        int idx = triIndices[i];
        const Triangle& tri = m_triangles[idx];
        AABB triBox = AABB::from_triangle(tri);
        bounds.min.x = fminf(bounds.min.x, triBox.min.x);
        bounds.min.y = fminf(bounds.min.y, triBox.min.y);
        bounds.min.z = fminf(bounds.min.z, triBox.min.z);
        bounds.max.x = fmaxf(bounds.max.x, triBox.max.x);
        bounds.max.y = fmaxf(bounds.max.y, triBox.max.y);
        bounds.max.z = fmaxf(bounds.max.z, triBox.max.z);
    }

    // Leaf node
    if (count <= LEAF_SIZE) {
        BVHNode node;
        node.bounds = bounds;
        node.leftChild = -1;
        node.rightChild = -1;
        node.firstTriIndex = start;
        node.triCount = count;
        m_nodes.push_back(node);
        return (int)m_nodes.size() - 1;
    }

    // Internal node: split along longest axis of centroid AABB
    AABB centroidBounds;
    centroidBounds.min = {FLT_MAX, FLT_MAX, FLT_MAX};
    centroidBounds.max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

    for (int i = start; i < start + count; ++i) {
        int idx = triIndices[i];
        const Triangle& tri = m_triangles[idx];
        Vec3 centroid = (tri.v0 + tri.v1 + tri.v2) * (1.0f / 3.0f);
        centroidBounds.min.x = fminf(centroidBounds.min.x, centroid.x);
        centroidBounds.min.y = fminf(centroidBounds.min.y, centroid.y);
        centroidBounds.min.z = fminf(centroidBounds.min.z, centroid.z);
        centroidBounds.max.x = fmaxf(centroidBounds.max.x, centroid.x);
        centroidBounds.max.y = fmaxf(centroidBounds.max.y, centroid.y);
        centroidBounds.max.z = fmaxf(centroidBounds.max.z, centroid.z);
    }

    Vec3 extents = centroidBounds.max - centroidBounds.min;
    int axis = 0;
    if (extents.y >= extents.x && extents.y >= extents.z) axis = 1;
    else if (extents.z >= extents.x && extents.z >= extents.y) axis = 2;

    // Sort triangles by centroid along the chosen axis
    std::sort(triIndices.begin() + start, triIndices.begin() + start + count,
        [this, axis](int a, int b) {
            const Triangle& ta = m_triangles[a];
            const Triangle& tb = m_triangles[b];
            Vec3 ca = (ta.v0 + ta.v1 + ta.v2) * (1.0f / 3.0f);
            Vec3 cb = (tb.v0 + tb.v1 + tb.v2) * (1.0f / 3.0f);
            float va = (axis == 0) ? ca.x : (axis == 1) ? ca.y : ca.z;
            float vb = (axis == 0) ? cb.x : (axis == 1) ? cb.y : cb.z;
            return va < vb;
        });

    int mid = start + count / 2;
    int leftIdx = build_node(triIndices, start, mid - start, depth + 1);
    int rightIdx = build_node(triIndices, mid, start + count - mid, depth + 1);

    BVHNode node;
    node.bounds = bounds;
    node.leftChild = leftIdx;
    node.rightChild = rightIdx;
    node.firstTriIndex = -1;
    node.triCount = 0;
    m_nodes.push_back(node);
    return (int)m_nodes.size() - 1;
}

void PhysicsWorld::query_node(int nodeIdx, const AABB& box, std::vector<int>& outIndices) const {
    if (nodeIdx < 0 || nodeIdx >= (int)m_nodes.size()) return;
    const BVHNode& node = m_nodes[nodeIdx];

    // Bounding box test
    if (!node.bounds.overlaps(box)) return;

    // Leaf: add triangles
    if (node.firstTriIndex >= 0) {
        for (int i = node.firstTriIndex; i < node.firstTriIndex + node.triCount; ++i) {
            outIndices.push_back(m_triIndices[i]);
        }
        return;
    }

    // Internal: recurse
    query_node(node.leftChild, box, outIndices);
    query_node(node.rightChild, box, outIndices);
}

void PhysicsWorld::build(const std::vector<Triangle>& triangles) {
    m_triangles = triangles;
    m_nodes.clear();
    m_root = -1;

    if (m_triangles.empty()) return;

    // Build index list
    m_triIndices.resize(m_triangles.size());
    for (size_t i = 0; i < m_triangles.size(); ++i)
        m_triIndices[i] = (int)i;

    m_root = build_node(m_triIndices, 0, (int)m_triIndices.size(), 0);
}

bool PhysicsWorld::capsule_hit(const Vec3& center, float radius, float halfHeight, HitResult& outHit) const {
    outHit.hit = false;
    outHit.penetration = 0.0f;
    outHit.normal = {0.0f, 1.0f, 0.0f};

    if (m_triangles.empty() || m_root < 0) return false;

    // ---- 1. Query BVH for candidate triangles ----
    AABB capsuleBox = AABB::from_capsule(center, radius, halfHeight);
    std::vector<int> candidates;
    candidates.reserve(128);
    query_node(m_root, capsuleBox, candidates);

    if (candidates.empty()) return false;

    // ---- 2. Run exact capsule–triangle distance on candidates ----
    Vec3 A = center - Vec3{0.0f, halfHeight, 0.0f};
    Vec3 B = center + Vec3{0.0f, halfHeight, 0.0f};

    float minDist = FLT_MAX;
    Vec3 bestSegPoint = center;
    Vec3 bestTriPoint = center;

    for (int idx : candidates) {
        const Triangle& tri = m_triangles[idx];
        Vec3 segP, triP;
        float dist = distance_segment_triangle(A, B, tri.v0, tri.v1, tri.v2, segP, triP);
        if (dist < minDist) {
            minDist = dist;
            bestSegPoint = segP;
            bestTriPoint = triP;
        }
    }

    if (minDist < radius) {
        outHit.hit = true;
        outHit.penetration = radius - minDist;
        Vec3 diff = bestSegPoint - bestTriPoint;
        float len = diff.length();
        if (len > 0.0001f)
            outHit.normal = diff * (1.0f / len);
        else
            outHit.normal = {0.0f, 1.0f, 0.0f};
        return true;
    }
    return false;
}