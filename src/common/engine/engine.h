#pragma once
#include "platform/platform.h"
#include "core/memory.h"
#include "core/math.h"
#include "core/camera.h"
#include "core/settings.h"
#include "renderer/renderer.h"
#include "world/level.h"
#include "engine/game_interface.h"
#include <memory>

class Engine
{
public:
    bool initialize();
    void shutdown();
    void run();

    void update(float deltaTime);
    void render(float deltaTime);
    void set_render_callback(void (*callback)(void*), void* userData);
    void on_resize(int width, int height);

    // Settings
    const Settings& get_settings() const { return m_settings; }
    bool reload_settings();

    void* get_window_handle() const { return platform.windowHandle; }
    Renderer* get_renderer() { return &renderer; }
    Camera* get_camera() { return &camera; }
    Level* get_level() { return &level; }
    const PlatformState& get_platform_state() const { return platform; }
    bool should_close() const { return platform_should_close(&platform); }

    void set_editor_mode(bool enabled);
    void set_game_mode(bool enabled) { m_gameMode = enabled; }

    void toggle_pause();
    bool is_paused() const { return m_paused; }

    int get_mouse_wheel_delta() {
        int val = platform.mouseWheelDelta;
        platform.mouseWheelDelta = 0;
        return val;
    }

    void lock_mouse(bool lock);
    bool is_mouse_locked() const { return m_mouseLocked; }

    // Mouse delta for game
    void get_mouse_delta(float& dx, float& dy);

    // ---- NEW: set the game instance ----
    void set_game(std::unique_ptr<IGame> game);

private:
    // ---- Core systems ----
    PlatformState platform;
    LinearAllocator globalAllocator;
    Renderer renderer;
    Camera camera;
    Level level;
    float lastFrameTime = 0.0f;

    // ---- Settings ----
    Settings m_settings;

    // ---- Callbacks ----
    void (*renderCallback)(void*) = nullptr;
    void* callbackData = nullptr;

    // ---- Mode ----
    bool m_isEditorMode = false;
    bool m_gameMode = false;
    bool m_paused = false;

    // ---- Game instance ----
    std::unique_ptr<IGame> m_game;

    // ---- Mouse input ----
    bool m_mouseLocked = false;
    int m_windowWidth = 800;
    int m_windowHeight = 600;
};