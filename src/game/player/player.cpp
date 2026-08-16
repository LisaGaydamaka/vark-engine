#include "player.h"
#include "core/settings.h"
#include <windows.h>
#include <cmath>
#include <algorithm>
#include <cstdio>

static inline float clampf(float v, float min, float max) {
    return (v < min) ? min : (v > max) ? max : v;
}
static inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}
static inline float dot(const Vec3& a, const Vec3& b) { return Vec3::dot(a, b); }

// Constructor
Player::Player(const PhysicsWorld* physics, Camera* camera, const Settings* settings, const PhysicsProfile& profile)
    : m_position{0,0,0}, m_velocity{0,0,0}, m_yaw(0), m_pitch(0),
      m_onGround(false), m_wantCrouch(false), m_requestUncrouch(false),
      m_crouchFactor(0), m_crouchFactorPrev(0), m_wantsRun(false),
      m_groundNormal{0,1,0}, m_wishDir{0,0,0}, m_wishSpeed(0),
      m_profile(profile), m_physics(physics), m_camera(camera), m_settings(settings) {}
// -----------------------------------------------------------------------------
// update – new physics pipeline (unchanged except crouch)
// -----------------------------------------------------------------------------
void Player::update(float dt)
{
    if (!m_physics || !m_camera) return;

    // ---- 1. Input and crouch ----
    process_input(dt);
    update_crouch(dt);   // now uses test_height and m_wantCrouch

    // ---- 2. Acceleration (ground/air) ----
    float accel = m_onGround ? m_profile.groundAccel : m_profile.airAccel;
    Vec3 targetVel = m_wishDir * m_wishSpeed;          // horizontal wish
    Vec3 deltaVel = targetVel - Vec3{m_velocity.x, 0, m_velocity.z};
    float maxDelta = accel * dt;
    if (deltaVel.length() > maxDelta && maxDelta > 0.0f) {
        deltaVel = deltaVel.normalized() * maxDelta;
    }
    m_velocity.x += deltaVel.x;
    m_velocity.z += deltaVel.z;

    // ---- 3. Gravity ----
    m_velocity.y += m_profile.gravity * dt;

    // ---- 4. Ground detection (downward sweep) ----
    check_ground();

    // ---- 5. If grounded: project velocity onto slope and apply friction ----
    if (m_onGround) {
        Vec3 N = m_groundNormal;

        // Remove any downward velocity component into the ground
        float velNormal = dot(m_velocity, N);
        if (velNormal < 0.0f) {
            m_velocity -= N * velNormal;
        }

        // Horizontal friction (deceleration)
        Vec3 horizVel = {m_velocity.x, 0, m_velocity.z};
        float speed = horizVel.length();
        if (speed > 0.001f) {
            float frictionDrop = m_profile.frictionCoeff * dt;
            if (speed <= frictionDrop) {
                horizVel = {0,0,0};
            } else {
                horizVel -= horizVel.normalized() * frictionDrop;
            }
            m_velocity.x = horizVel.x;
            m_velocity.z = horizVel.z;
        }
    }

    // ---- 6. Resolve collisions (sweep + slide + step‑up) ----
    Vec3 newPos = m_position;
    Vec3 newVel = m_velocity;
    resolve_collision(newPos, newVel, dt);
    m_position = newPos;
    m_velocity = newVel;

    // ---- 7. Camera update ----
    float halfH = get_current_half_height();
    float eyeY = m_position.y + halfH + m_profile.radius - 0.1f;
    m_camera->set_position({m_position.x, eyeY, m_position.z});
    m_camera->set_yaw(m_yaw);
    m_camera->set_pitch(m_pitch);
}

// -----------------------------------------------------------------------------
// update_crouch – dynamic height with auto-uncrouch (new logic)
// -----------------------------------------------------------------------------
void Player::update_crouch(float dt)
{
    // Determine target factor based on desired crouch state
    float targetFactor = m_wantCrouch ? 1.0f : 0.0f;
    float speed = m_profile.crouchLerpSpeed;
    float newFactor = m_crouchFactor;

    if (m_wantCrouch) {
        // Crouching: move towards 1.0
        newFactor = (std::min)(m_crouchFactor + speed * dt, 1.0f);
    } else {
        // Standing up: move towards 0.0, but only if it fits
        float desired = (std::max)(m_crouchFactor - speed * dt, 0.0f);
        if (test_height(desired)) {
            newFactor = desired;
        } else {
            // Can't go that low; stay at current height
            newFactor = m_crouchFactor;
        }
    }

    // Only apply the change if the new factor is valid (double‑check)
    if (test_height(newFactor)) {
        m_crouchFactor = newFactor;
    }

    // If we reached full stand, clear the request
    if (m_crouchFactor <= 0.001f && !m_wantCrouch) {
        m_requestUncrouch = false;
    }

    // Adjust vertical position to keep feet planted
    float oldHalf = m_profile.height * 0.5f * (1.0f - m_crouchFactorPrev) +
                    m_profile.crouchHeight * 0.5f * m_crouchFactorPrev;
    float newHalf = get_current_half_height();
    m_position.y += (newHalf - oldHalf);
    m_crouchFactorPrev = m_crouchFactor;
}

// -----------------------------------------------------------------------------
// test_height – checks if a capsule with given crouch factor collides with world
// -----------------------------------------------------------------------------
bool Player::test_height(float crouchFactor) const
{
    if (!m_physics) return true;  // no physics, assume ok

    float halfH = lerp(m_profile.height, m_profile.crouchHeight, crouchFactor) * 0.5f;
    float oldHalf = get_current_half_height();
    Vec3 newCenter = m_position + Vec3{0, halfH - oldHalf, 0}; // keep feet at same Y
    HitResult hit;
    return !m_physics->capsule_hit(newCenter, m_profile.radius, halfH, hit);
}

// -----------------------------------------------------------------------------
// process_input – crouch toggling and movement (new crouch logic)
// -----------------------------------------------------------------------------
void Player::process_input(float dt)
{
    (void)dt;

    // ---- Crouch toggle ----
    static bool cWasDown = false;
    bool cDown = m_settings->is_key_down(Action::Crouch);
    if (cDown && !cWasDown) {
        if (m_wantCrouch) {
            m_wantCrouch = false;
            m_requestUncrouch = true;
        } else {
            m_wantCrouch = true;
            m_requestUncrouch = false;
        }
    }
    cWasDown = cDown;

    // ---- Run / walk speed ----
    m_wantsRun = m_settings->is_key_down(Action::Run);
    float speed;
    if (m_crouchFactor > 0.5f)      speed = m_profile.crouchSpeed;
    else if (m_wantsRun)            speed = m_profile.runSpeed;
    else                            speed = m_profile.walkSpeed;

    // ---- Movement direction ----
    Vec3 forward = { sinf(m_yaw), 0, cosf(m_yaw) };
    Vec3 right   = { cosf(m_yaw), 0, -sinf(m_yaw) };
    Vec3 wish = {0,0,0};
    if (m_settings->is_key_down(Action::MoveForward)) wish = wish + forward;
    if (m_settings->is_key_down(Action::MoveBackward)) wish = wish - forward;
    if (m_settings->is_key_down(Action::MoveLeft)) wish = wish - right;
    if (m_settings->is_key_down(Action::MoveRight)) wish = wish + right;

    float len = wish.length();
    if (len > 0.001f) {
        m_wishDir = wish * (1.0f / len);
        m_wishSpeed = speed;
    } else {
        m_wishDir = {0,0,0};
        m_wishSpeed = 0;
    }

    // ---- Jump ----
    if (m_settings->is_key_down(Action::Jump) && m_onGround) {
        m_velocity.y = m_profile.jumpSpeed;
        m_onGround = false;
    }
}

// -----------------------------------------------------------------------------
// check_ground – downward sweep, exact push‑out (unchanged)
// -----------------------------------------------------------------------------
void Player::check_ground()
{
    if (!m_physics) return;

    float halfH = get_current_half_height();
    const float margin = 0.005f; // tiny downward offset to catch ground

    // Test the actual player capsule, shifted down by 'margin'
    // Vec3 testPos = m_position + Vec3{0, -margin, 0};
    Vec3 testPos = m_position + Vec3{0, -margin, 0};
    HitResult hit;

    if (m_physics->capsule_hit(testPos, m_profile.radius, halfH, hit))
    {
        float angle = acosf(dot(hit.normal, Vec3{0,1,0}));
        if (angle <= m_profile.walkableSlopeAngle)
        {
            m_onGround = true;
            m_groundNormal = hit.normal;

            // 1. Push out exactly by the reported penetration
            if (hit.penetration > 0.0f)
            {
                m_position += hit.normal * hit.penetration;

                // 2. Kill any downward velocity along the normal
                float velNormal = dot(m_velocity, hit.normal);
                if (velNormal < 0.0f)
                    m_velocity -= hit.normal * velNormal;
            }
            return;
        }
        else
        {
            m_onGround = false;
        }
    }
    else
    {
        m_onGround = false;
    }
}

// -----------------------------------------------------------------------------
// resolve_collision – step‑up only when moving (unchanged)
// -----------------------------------------------------------------------------
void Player::resolve_collision(Vec3& pos, Vec3& vel, float dt)
{
    const int substeps = 4;
    float subDt = dt / substeps;

    for (int step = 0; step < substeps; ++step) {
        Vec3 delta = vel * subDt;
        Vec3 newPos = pos + delta;

        if (!try_move(newPos, delta, subDt)) {
            HitResult hit;
            if (m_physics->capsule_hit(pos, m_profile.radius, get_current_half_height(), hit)) {
                Vec3 N = hit.normal;
                float velNormal = dot(vel, N);
                Vec3 velTangent = vel - N * velNormal;

                float angle = acosf(dot(N, Vec3{0,1,0}));
                float slopeDeg = angle * 180.0f / 3.14159265f;

                if (slopeDeg > m_profile.walkableSlopeAngle) {
                    // ---- Unwalkable: wall slide ----
                    vel = velTangent;
                    Vec3 gravityTangent = Vec3{0, m_profile.gravity, 0} - N * dot(Vec3{0, m_profile.gravity, 0}, N);
                    vel += gravityTangent * subDt;
                    float spd = vel.length();
                    if (spd > m_profile.maxSlideSpeed)
                        vel = vel * (m_profile.maxSlideSpeed / spd);
                    m_onGround = false;
                } else {
                    // ---- Walkable slope ----
                    if (N.y > 0.0f && velNormal < 0.0f) {
                        // Step‑up only if player is actively moving
                        if (m_wishSpeed > 0.0f && m_wishDir.length() > 0.001f) {
                            // Step‑up (tangent projection)
                            float speed = vel.length();
                            Vec3 tangent = vel - N * velNormal;
                            if (tangent.length() < 0.0001f) {
                                Vec3 horiz = {vel.x, 0, vel.z};
                                if (horiz.length() > 0.001f)
                                    tangent = horiz.normalized() * speed;
                                else
                                    tangent = Vec3{1,0,0} * speed;
                            } else {
                                tangent = tangent.normalized() * speed;
                            }
                            const float MIN_CLIMB = 0.3f;
                            if (tangent.y < MIN_CLIMB) tangent.y = MIN_CLIMB;
                            vel = tangent;
                        } else {
                            // No input: just project onto slope, no step‑up
                            if (velNormal < 0.0f) velNormal = 0.0f;
                            vel = velTangent + N * velNormal;
                        }
                    } else {
                        // Floor contact (hitting from above)
                        if (N.y > 0.5f) {
                            if (velNormal < 0.0f) velNormal = 0.0f;
                            vel = velTangent + N * velNormal;
                            m_onGround = true;
                            m_groundNormal = N;
                        } else {
                            // Side of a walkable slope – wall slide
                            vel = velTangent;
                        }
                    }
                }

                // Push out
                if (dot(vel, N) < 0.0f)
                    vel += N * (-dot(vel, N));
                const float EPSILON = 0.001f;
                pos += N * (hit.penetration + EPSILON);

            } else {
                pos = newPos;
            }
        } else {
            pos = newPos;
        }
    }
}

// -----------------------------------------------------------------------------
// try_move – static capsule overlap test (unchanged)
// -----------------------------------------------------------------------------
bool Player::try_move(Vec3& newPos, const Vec3& delta, float dt)
{
    (void)dt;
    if (!m_physics) return false;
    HitResult hit;
    if (!m_physics->capsule_hit(newPos, m_profile.radius, get_current_half_height(), hit)) {
        return true;
    }
    return false;
}

// -----------------------------------------------------------------------------
// get_current_half_height & wireframe (unchanged)
// -----------------------------------------------------------------------------
float Player::get_current_half_height() const
{
    float h = lerp(m_profile.height, m_profile.crouchHeight, m_crouchFactor);
    return h * 0.5f;
}

void Player::get_capsule_wireframe(std::vector<Vertex>& vertices) const
{
    vertices.clear();
    const int segments = 16;
    const int numRings = 20;
    float half = get_current_half_height();
    float r = m_profile.radius;
    float totalH = 2.0f * (half + r);
    float startY = -half - r;

    std::vector<std::vector<Vec3>> rings(numRings);
    for (int ringIdx = 0; ringIdx < numRings; ++ringIdx) {
        float t = (float)ringIdx / (numRings - 1);
        float y = startY + t * totalH;
        float absY = fabsf(y);
        float radiusAtY = (absY <= half) ? r : sqrtf(r*r - (absY-half)*(absY-half));
        if (radiusAtY < 0) radiusAtY = 0;
        Vec3 center = m_position + Vec3{0, y, 0};
        for (int i = 0; i < segments; ++i) {
            float angle = 2.0f * 3.14159265f * i / segments;
            rings[ringIdx].push_back(center + Vec3{radiusAtY * cosf(angle), 0, radiusAtY * sinf(angle)});
        }
    }

    auto addLine = [&](const Vec3& a, const Vec3& b) {
        Vertex v1{a.x,a.y,a.z,0,0,0,1,0}, v2{b.x,b.y,b.z,0,0,0,1,0};
        vertices.push_back(v1); vertices.push_back(v2);
    };

    for (int ring = 0; ring < numRings; ++ring)
        for (int i = 0; i < segments; ++i)
            addLine(rings[ring][i], rings[ring][(i+1)%segments]);

    for (int ring = 0; ring < numRings-1; ++ring)
        for (int i = 0; i < segments; ++i)
            addLine(rings[ring][i], rings[ring+1][i]);
}