#include "math.h"

// ---- Vec3 ----
Vec3 Vec3::operator+(const Vec3& other) const
{
    return { x + other.x, y + other.y, z + other.z };
}

Vec3 Vec3::operator-(const Vec3& other) const
{
    return { x - other.x, y - other.y, z - other.z };
}

Vec3 Vec3::operator*(float scalar) const
{
    return { x * scalar, y * scalar, z * scalar };
}

Vec3& Vec3::operator+=(const Vec3& other)
{
    x += other.x; y += other.y; z += other.z;
    return *this;
}

Vec3& Vec3::operator-=(const Vec3& other)
{
    x -= other.x; y -= other.y; z -= other.z;
    return *this;
}

Vec3& Vec3::operator*=(float scalar)
{
    x *= scalar; y *= scalar; z *= scalar;
    return *this;
}

float Vec3::length() const
{
    return sqrtf(x*x + y*y + z*z);
}

Vec3 Vec3::normalized() const
{
    float len = length();
    if (len < 0.0001f) return {0.0f, 0.0f, 0.0f};
    return { x/len, y/len, z/len };
}

Vec3 Vec3::cross(const Vec3& a, const Vec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float Vec3::dot(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// ---- Mat4 ----
static Mat4 mat4_identity()
{
    Mat4 result = {};
    result.m[0][0] = 1.0f;
    result.m[1][1] = 1.0f;
    result.m[2][2] = 1.0f;
    result.m[3][3] = 1.0f;
    return result;
}

Mat4 Mat4::identity() { return mat4_identity(); }

Mat4 Mat4::operator*(const Mat4& b) const
{
    Mat4 result = {};
    for (int row = 0; row < 4; row++)
    {
        for (int col = 0; col < 4; col++)
        {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++)
                sum += m[row][k] * b.m[k][col];
            result.m[row][col] = sum;
        }
    }
    return result;
}

Mat4 mat4_translation(float x, float y, float z)
{
    Mat4 result = mat4_identity();
    result.m[3][0] = x;
    result.m[3][1] = y;
    result.m[3][2] = z;
    return result;
}

Mat4 mat4_rotation_z(float angle)
{
    float c = cosf(angle);
    float s = sinf(angle);
    Mat4 result = mat4_identity();
    result.m[0][0] = c;  result.m[0][1] = s;
    result.m[1][0] = -s; result.m[1][1] = c;
    return result;
}

Mat4 mat4_rotation_x(float angle)
{
    float c = cosf(angle);
    float s = sinf(angle);
    Mat4 result = mat4_identity();
    result.m[1][1] = c;  result.m[1][2] = s;
    result.m[2][1] = -s; result.m[2][2] = c;
    return result;
}

Mat4 mat4_rotation_y(float angle)
{
    float c = cosf(angle);
    float s = sinf(angle);
    Mat4 result = mat4_identity();
    result.m[0][0] = c;  result.m[0][2] = -s;
    result.m[2][0] = s;  result.m[2][2] = c;
    return result;
}

Mat4 mat4_perspective(float fov, float aspect, float nearZ, float farZ)
{
    float tanHalfFov = tanf(fov / 2.0f);
    Mat4 result = {};
    result.m[0][0] = 1.0f / (aspect * tanHalfFov);
    result.m[1][1] = 1.0f / tanHalfFov;
    result.m[2][2] = farZ / (farZ - nearZ);
    result.m[2][3] = 1.0f;
    result.m[3][2] = -(farZ * nearZ) / (farZ - nearZ);
    return result;
}

// ---- Triangle distance ----
float distSqPointTriangle(const Vec3& p, const Vec3& a, const Vec3& b, const Vec3& c)
{
    Vec3 ab = b - a;
    Vec3 ac = c - a;
    Vec3 ap = p - a;

    float d1 = Vec3::dot(ab, ap);
    float d2 = Vec3::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return Vec3::dot(ap, ap);

    Vec3 bp = p - b;
    float d3 = Vec3::dot(ab, bp);
    float d4 = Vec3::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return Vec3::dot(bp, bp);

    Vec3 cp = p - c;
    float d5 = Vec3::dot(ab, cp);
    float d6 = Vec3::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return Vec3::dot(cp, cp);

    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        float v = d1 / (d1 - d3);
        Vec3 closest = a + ab * v;
        Vec3 diff = p - closest;
        return Vec3::dot(diff, diff);
    }

    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        float w = d2 / (d2 - d6);
        Vec3 closest = a + ac * w;
        Vec3 diff = p - closest;
        return Vec3::dot(diff, diff);
    }

    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        Vec3 closest = b + (c - b) * w;
        Vec3 diff = p - closest;
        return Vec3::dot(diff, diff);
    }

    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    Vec3 closest = a + ab * v + ac * w;
    Vec3 diff = p - closest;
    return Vec3::dot(diff, diff);
}