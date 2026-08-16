#pragma once
#include "math.h"
#include "actions.h"
#include <string>
#include <unordered_map>

struct Settings
{
    // Display
    int windowWidth  = 1280;
    int windowHeight = 720;
    bool fullscreen  = false;

    // Camera
    float fovDegrees = 60.0f;

    // Input
    float mouseSensitivity = 0.002f;

    // Level
    std::string levelFile = "assets/levels/test.vmb";

    // ---- Keybinds ----
    std::unordered_map<Action, int> keybinds;

    bool load(const char* filepath);
    bool save(const char* filepath) const;
    void set_defaults();

    // Helper: get virtual key code for an action
    int get_key(Action action) const;

    // Helper: check if action is currently pressed (using GetAsyncKeyState)
    bool is_key_down(Action action) const;
};