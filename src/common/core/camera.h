// src/common/core/camera.h
#pragma once
#include "math.h"

class Camera
{
public:
    Camera();

    // First-person
    void set_position(const Vec3& pos);
    void move(const Vec3& delta); // x=right, y=up, z=forward

    // Orientation
    void set_yaw(float yaw);
    void set_pitch(float pitch);

    Vec3 get_position() const;
    Vec3 get_forward() const;
    Vec3 get_right() const;
    Vec3 get_up() const;
    Mat4 get_view_matrix() const;

    float get_yaw() const;
    float get_pitch() const;

private:
    Vec3 m_position;
    float m_yaw;
    float m_pitch;
};