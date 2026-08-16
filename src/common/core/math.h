#pragma once
#include <cmath>

struct Vec2
{
    float x, y;
    Vec2() : x(0.0f), y(0.0f) {}
    Vec2(float _x, float _y) : x(_x), y(_y) {}

    // ---- Added operators ----
    Vec2 operator+(const Vec2& other) const { return Vec2(x + other.x, y + other.y); }
    Vec2 operator-(const Vec2& other) const { return Vec2(x - other.x, y - other.y); }
    Vec2 operator*(float scalar) const { return Vec2(x * scalar, y * scalar); }
    Vec2& operator+=(const Vec2& other) { x += other.x; y += other.y; return *this; }
    Vec2& operator-=(const Vec2& other) { x -= other.x; y -= other.y; return *this; }
    Vec2& operator*=(float scalar) { x *= scalar; y *= scalar; return *this; }
};
// Free function for left-side scalar multiplication
inline Vec2 operator*(float scalar, const Vec2& v) { return v * scalar; }

// ---- Vec3 (keep your existing one) ----
struct Vec3
{
    float x, y, z;
    Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
    Vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    Vec3 operator+(const Vec3& other) const;
    Vec3 operator-(const Vec3& other) const;
    Vec3 operator*(float scalar) const;
    Vec3& operator+=(const Vec3& other);
    Vec3& operator-=(const Vec3& other);
    Vec3& operator*=(float scalar);

    float length() const;
    Vec3 normalized() const;
    
    static Vec3 cross(const Vec3& a, const Vec3& b);
    static float dot(const Vec3& a, const Vec3& b);
};

inline Vec3 operator*(float scalar, const Vec3& v) {
    return v * scalar;
}

// ---- Mat4 ----
struct Mat4
{
    float m[4][4]; // Row-major
    static Mat4 identity();
    Mat4 operator*(const Mat4& other) const;
};

// Matrix creation functions
Mat4 mat4_translation(float x, float y, float z);
Mat4 mat4_rotation_z(float angle);
Mat4 mat4_rotation_x(float angle);
Mat4 mat4_rotation_y(float angle);
Mat4 mat4_perspective(float fov, float aspect, float nearZ, float farZ);

// ---- Triangle distance ----
float distSqPointTriangle(const Vec3& p, const Vec3& a, const Vec3& b, const Vec3& c);