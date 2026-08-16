#pragma once
#include "core/math.h"
#include "core/camera.h"
#include "core/geometry.h"
#include "core/physics_profile.h"
#include "physics/physics_world.h"
#include "core/settings.h"      // NEW
#include <vector>

class Player
{
public:
    Player(const PhysicsWorld* physics, Camera* camera, const Settings* settings, const PhysicsProfile& profile = PhysicsProfile());
    ~Player() = default;

    // Accessors
    void set_position(const Vec3& pos) { m_position = pos; }
    void set_yaw(float yaw)            { m_yaw = yaw; }
    void set_pitch(float pitch)        { m_pitch = pitch; }
    Vec3 get_position() const          { return m_position; }
    float get_yaw() const              { return m_yaw; }
    float get_pitch() const            { return m_pitch; }

    void update(float deltaTime);

    // Debug wireframe
    void get_capsule_wireframe(std::vector<Vertex>& vertices) const;

private:
    // Core physics functions
    void apply_physics(float dt);
    void resolve_collision(Vec3& pos, Vec3& vel, float dt);
    void check_ground();
    bool try_move(Vec3& newPos, const Vec3& delta, float dt);

    // Input and state
    void process_input(float dt);
    void update_crouch(float dt);
    bool test_height(float crouchFactor) const;
    float get_current_half_height() const;

    // Data
    Vec3 m_position;
    Vec3 m_velocity;
    float m_yaw;
    float m_pitch;
    bool m_onGround;
    bool m_wantCrouch;
    bool m_requestUncrouch;
    float m_crouchFactor;
    float m_crouchFactorPrev;
    bool m_wantsRun;
    Vec3 m_groundNormal;

    Vec3 m_wishDir;
    float m_wishSpeed;

    PhysicsProfile m_profile;
    const PhysicsWorld* m_physics;
    Camera* m_camera;
    const Settings* m_settings;   // NEW
};