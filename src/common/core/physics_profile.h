#pragma once

struct PhysicsProfile
{
    // Capsule
    float height = 3.5f;
    float crouchHeight = 2.0f;
    float radius = 1.25f;

    // Speeds (slightly higher)
    float walkSpeed = 12.0f;      // was 10
    float runSpeed   = 18.0f;     // was 15
    float crouchSpeed = 7.0f;     // was 6

    // Acceleration (even snappier)
    float groundAccel = 120.0f;   // was 100
    float airAccel = 40.0f;       // was 10

    // Friction deceleration (quick stops)
    float frictionCoeff = 24.0f;  // was 20 (to match higher speeds)

    // Jump and gravity – fast fall, decent height
    float jumpSpeed = 19.0f;
    float gravity = -45.0f;

    // Slopes
    float walkableSlopeAngle = 45.0f;
    float maxSlideSpeed = 25.0f;
    float stepHeight = 0.5f;
    float groundSnapDistance = 0.1f;
    float crouchLerpSpeed = 10.0f;
};