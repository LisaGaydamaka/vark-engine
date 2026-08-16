// src/editor/editor_camera.cpp
#include "editor_camera.h"
#include <algorithm>
#include <cmath>

EditorCamera::EditorCamera() {
    update();
}

void EditorCamera::set_target(const Vec3& target) {
    m_target = target;
    update();
}

void EditorCamera::set_distance(float dist) {
    m_distance = (dist < 0.1f) ? 0.1f : dist;
    update();
}

void EditorCamera::set_yaw(float yaw) {
    m_yaw = yaw;
    update();
}

void EditorCamera::set_pitch(float pitch) {
    m_pitch = std::max(-1.5f, std::min(1.5f, pitch));
    update();
}

void EditorCamera::update() {
    // Compute position from orbit parameters
    float x = m_distance * cosf(m_pitch) * sinf(m_yaw);
    float y = m_distance * sinf(m_pitch);
    float z = m_distance * cosf(m_pitch) * cosf(m_yaw);
    Vec3 pos = m_target + Vec3{x, y, z};

    m_camera.set_position(pos);

    // Look-at direction
    Vec3 dir = m_target - pos;

    // Yaw: angle on XZ plane
    float yaw = atan2f(dir.x, dir.z);

    // Pitch: elevation angle (atan2(-dy, sqrt(dx²+dz²)))
    float horiz = sqrtf(dir.x * dir.x + dir.z * dir.z);
    float pitch = atan2f(-dir.y, horiz);

    m_camera.set_yaw(yaw);
    m_camera.set_pitch(pitch);
}

void EditorCamera::orbit(float dx, float dy) {
    m_yaw += dx * 0.01f;
    m_pitch += dy * 0.01f;
    m_pitch = std::max(-1.5f, std::min(1.5f, m_pitch));
    update();
}

void EditorCamera::pan(float dx, float dy) {
    float speed = m_distance * 0.003f;

    // Right direction: direction of increasing yaw (horizontal tangent)
    Vec3 right = { cosf(m_yaw), 0.0f, -sinf(m_yaw) };

    // Up direction: direction of increasing pitch (meridian tangent)
    Vec3 up = { -sinf(m_pitch) * sinf(m_yaw),
                 cosf(m_pitch),
                -sinf(m_pitch) * cosf(m_yaw) };

    // Pan the target along these basis vectors
    m_target = m_target - right * (-dx * speed) + up * (dy * speed);
    update();
}

void EditorCamera::zoom(float delta) {
    m_distance -= delta * 0.02f;
    if (m_distance < 0.1f) m_distance = 0.1f;
    update();
}