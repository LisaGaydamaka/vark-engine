// src/editor/editor_camera.h
#pragma once
#include "core/math.h"
#include "core/camera.h"

class EditorCamera {
public:
    EditorCamera();
    ~EditorCamera() = default;

    // ---- Access to underlying camera for rendering ----
    Camera* get_camera() { return &m_camera; }
    const Camera* get_camera() const { return &m_camera; }

    // ---- Orbit controls ----
    void set_target(const Vec3& target);
    Vec3 get_target() const { return m_target; }

    void set_distance(float distance);
    float get_distance() const { return m_distance; }

    void set_yaw(float yaw);
    float get_yaw() const { return m_yaw; }

    void set_pitch(float pitch);
    float get_pitch() const { return m_pitch; }

    // ---- Combined update ----
    void update();

    // ---- Interaction helpers ----
    void orbit(float dx, float dy);   // left drag
    void pan(float dx, float dy);     // middle drag
    void zoom(float delta);           // mouse wheel

private:
    Camera m_camera;
    Vec3 m_target = {0,0,0};
    float m_distance = 20.0f;
    float m_yaw = 0.0f;
    float m_pitch = 0.5f;
};