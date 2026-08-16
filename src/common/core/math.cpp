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
