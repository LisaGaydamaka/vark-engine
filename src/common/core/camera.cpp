// src/common/core/camera.cpp
#include "camera.h"
#include <cmath>

Camera::Camera()
    : m_position{0.0f, 0.0f, 0.0f}
    , m_yaw(0.0f)
    , m_pitch(0.0f)
{
}

void Camera::set_position(const Vec3& pos) { m_position = pos; }
void Camera::move(const Vec3& delta)
{
    Vec3 fwd = get_forward();
    Vec3 rgt = get_right();
    Vec3 up  = Vec3{0.0f, 1.0f, 0.0f};
    m_position = m_position + rgt * delta.x + up * delta.y + fwd * delta.z;
}

void Camera::set_yaw(float yaw)   { m_yaw = yaw; }
void Camera::set_pitch(float pitch) { m_pitch = pitch; }
float Camera::get_yaw() const     { return m_yaw; }
float Camera::get_pitch() const   { return m_pitch; }

Vec3 Camera::get_position() const
{
    return m_position;
}

Vec3 Camera::get_forward() const
{
    float cy = cosf(m_yaw);
    float sy = sinf(m_yaw);
    float cp = cosf(m_pitch);
    float sp = sinf(m_pitch);
    // Standard FPS forward direction (points along +Z when yaw=pitch=0)
    return Vec3{ sy * cp, sp, cy * cp };
}

Vec3 Camera::get_right() const
{
    Vec3 fwd = get_forward();
    Vec3 up  = Vec3{0.0f, 1.0f, 0.0f};
    Vec3 r = Vec3::cross(up, fwd);
    return r.normalized();
}

Vec3 Camera::get_up() const
{
    Vec3 fwd = get_forward();
    Vec3 rgt = get_right();
    return Vec3::cross(fwd, rgt).normalized();
}

Mat4 Camera::get_view_matrix() const
{
    // First‑person: Translate first, then rotate
    Mat4 translation = Mat4::identity();
    translation.m[3][0] = -m_position.x;
    translation.m[3][1] = -m_position.y;
    translation.m[3][2] = -m_position.z;

    Mat4 rotY = Mat4::identity();
    float cy = cosf(-m_yaw);
    float sy = sinf(-m_yaw);
    rotY.m[0][0] = cy;  rotY.m[0][2] = -sy;
    rotY.m[2][0] = sy;  rotY.m[2][2] = cy;

    Mat4 rotX = Mat4::identity();
    float cp = cosf(-m_pitch);
    float sp = sinf(-m_pitch);
    rotX.m[1][1] = cp;  rotX.m[1][2] = sp;
    rotX.m[2][1] = -sp; rotX.m[2][2] = cp;

    return translation * rotY * rotX;
}