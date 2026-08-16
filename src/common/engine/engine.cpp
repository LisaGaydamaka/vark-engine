#include "engine.h"
#include "core/settings.h"
#include "core/logger.h"
#include <windows.h>
#include <cstdio>
#include <cmath>

static inline float clampf(float value, float minVal, float maxVal)
{
    if (value < minVal) return minVal;
    if (value > maxVal) return maxVal;
    return value;
}

// ---- Mouse lock, pause, editor mode ----
void Engine::lock_mouse(bool lock)
{
    if (lock == m_mouseLocked) return;
    HWND hwnd = (HWND)platform.windowHandle;
    if (lock) {
        RECT rect;
        GetClientRect(hwnd, &rect);
        MapWindowPoints(hwnd, nullptr, (POINT*)&rect, 2);
        ClipCursor(&rect);
        ShowCursor(FALSE);
        m_mouseLocked = true;
    } else {
        ClipCursor(nullptr);
        ShowCursor(TRUE);
        m_mouseLocked = false;
    }
}

void Engine::toggle_pause()
{
    m_paused = !m_paused;
    if (m_game) {
        m_game->on_pause_toggle();
    } else {
        // Fallback: just lock/unlock mouse
        if (m_paused) {
            lock_mouse(false);
            LOG_INFO("[PAUSE] Game paused");
        } else {
            if (m_gameMode) {
                lock_mouse(true);
                HWND hwnd = (HWND)platform.windowHandle;
                RECT rect;
                GetClientRect(hwnd, &rect);
                POINT center = { rect.right / 2, rect.bottom / 2 };
                ClientToScreen(hwnd, &center);
                SetCursorPos(center.x, center.y);
            }
            LOG_INFO("[PAUSE] Game resumed");
        }
    }
}

void Engine::set_editor_mode(bool enabled)
{
    m_isEditorMode = enabled;
    m_gameMode = !enabled;
    if (m_game) m_game->on_editor_mode(enabled);
    if (m_gameMode) {
        if (m_paused) {
            m_paused = false;
        }
        lock_mouse(true);
        HWND hwnd = (HWND)platform.windowHandle;
        RECT rect;
        GetClientRect(hwnd, &rect);
        POINT center = { rect.right / 2, rect.bottom / 2 };
        ClientToScreen(hwnd, &center);
        SetCursorPos(center.x, center.y);
    } else {
        lock_mouse(false);
    }
    LOG_INFO("[ENGINE] set_editor_mode(%d) -> m_gameMode = %d, m_paused = %d", enabled, m_gameMode, m_paused);
}

// ---- Reload settings ----
bool Engine::reload_settings()
{
    if (!m_settings.load("settings.ini")) {
        LOG_WARN("Failed to reload settings, keeping current.");
        return false;
    }
    LOG_INFO("Settings reloaded: FOV=%.1f, Sens=%.3f",
           m_settings.fovDegrees, m_settings.mouseSensitivity);
    return true;
}

// ---- Set game ----
void Engine::set_game(std::unique_ptr<IGame> game)
{
    if (m_game) {
        m_game->shutdown();
        m_game.reset();
    }
    m_game = std::move(game);
    if (m_game) {
        if (!m_game->initialize(this)) {
            LOG_ERROR("Failed to initialize game!");
            m_game.reset();
        }
    }
}

// ---- Initialization ----
bool Engine::initialize()
{
    globalAllocator.init(1024 * 1024);
    Logger::instance().init();

    // ---- 1. Load settings ----
    if (!m_settings.load("settings.ini")) {
        LOG_INFO("No settings.ini found, creating default.");
        m_settings.save("settings.ini");
    } else {
        LOG_INFO("Settings loaded: %dx%d, FOV=%.1f, Fullscreen=%d",
               m_settings.windowWidth, m_settings.windowHeight,
               m_settings.fovDegrees, m_settings.fullscreen);
    }

    // ---- 2. Create window ----
    LOG_INFO("Creating window...");
    if (!platform_create_window(&platform,
                                m_settings.windowWidth,
                                m_settings.windowHeight,
                                "Vibe Engine",
                                m_settings.fullscreen)) {
        LOG_ERROR("Failed to create window!");
        return false;
    }

    HWND hwnd = (HWND)platform.windowHandle;
    LOG_INFO("Initializing renderer with %dx%d...", platform.width, platform.height);

    // ---- 3. Initialize renderer ----
    if (!renderer.initialize(hwnd, platform.width, platform.height)) {
        LOG_ERROR("Failed to initialize renderer!");
        return false;
    }

    LOG_INFO("Renderer initialized successfully.");
    LOG_INFO("Renderer device pointer: %p", renderer.get_device());

    // ---- 4. Load level ----
    LOG_INFO("Loading level from: '%s'", m_settings.levelFile.c_str());
    if (!level.build(&renderer, m_settings.levelFile.c_str())) {
        LOG_ERROR("Failed to build level!");
        return false;
    }

    m_windowWidth = platform.width;
    m_windowHeight = platform.height;

    m_paused = false;
    set_editor_mode(false);

    LOG_INFO("Engine initialized. Settings: FOV=%.1f, Sens=%.3f",
           m_settings.fovDegrees, m_settings.mouseSensitivity);
    LOG_INFO("WASD to move, mouse to look, Space to jump, C to crouch, Shift to run, ESC to pause.");
    return true;
}

// ---- Shutdown ----
void Engine::shutdown()
{
    if (m_game) {
        m_game->shutdown();
        m_game.reset();
    }

    level.shutdown(&renderer);
    lock_mouse(false);
    renderer.shutdown();
    platform_destroy_window(&platform);
    globalAllocator.shutdown();
}

// ---- Mouse delta ----
void Engine::get_mouse_delta(float& dx, float& dy)
{
    HWND hwnd = (HWND)platform.windowHandle;
    POINT cursor;
    GetCursorPos(&cursor);
    RECT rect;
    GetClientRect(hwnd, &rect);
    POINT center = { rect.right / 2, rect.bottom / 2 };
    ClientToScreen(hwnd, &center);
    dx = (float)(cursor.x - center.x);
    dy = (float)(cursor.y - center.y);
    SetCursorPos(center.x, center.y);

    float sens = m_settings.mouseSensitivity;
    dx *= sens;
    dy *= sens;
}

// ---- Update ----
void Engine::update(float deltaTime)
{
    platform_pump_messages(&platform);
    if (platform_should_close(&platform)) return;

    // ---- Pause (ESC) ----
    static bool escWasDown = false;
    bool escDown = m_settings.is_key_down(Action::Pause);
    if (escDown && !escWasDown) {
        toggle_pause();
    }
    escWasDown = escDown;

    // ---- Debug toggle (F3) ----
    static bool f3WasDown = false;
    bool f3Down = m_settings.is_key_down(Action::DebugToggle);
    if (f3Down && !f3WasDown) {
        level.set_debug_mode(!level.is_debug_mode());
        LOG_INFO("Debug mode: %s", level.is_debug_mode() ? "ON" : "OFF");
    }
    f3WasDown = f3Down;

    if (m_paused) return;

    if (m_game) {
        m_game->update(deltaTime);
    }
}


// ---- Render ----
void Engine::render(float deltaTime)
{
    renderer.begin_frame();

    Mat4 world = Mat4::identity();
    Mat4 view = camera.get_view_matrix();
    float aspect = (float)platform.width / (float)platform.height;
    float fovRad = m_settings.fovDegrees * 3.14159265f / 180.0f;
    Mat4 proj = mat4_perspective(fovRad, aspect, 0.1f, 100.0f);
    Mat4 mvp = world * view * proj;

    renderer.set_transform(mvp);
    level.render(&renderer);

    // Game-specific overlay (wireframe etc.)
    if (m_game) {
        m_game->render();
    }

    if (renderCallback) renderCallback(callbackData);

    renderer.end_frame();
}

// ---- Run loop ----
void Engine::run()
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);

    LARGE_INTEGER lastTime;
    QueryPerformanceCounter(&lastTime);

    while (!platform_should_close(&platform)) {
        LARGE_INTEGER currentTime;
        QueryPerformanceCounter(&currentTime);
        float deltaTime = (float)(currentTime.QuadPart - lastTime.QuadPart) / (float)frequency.QuadPart;
        lastTime = currentTime;

        if (deltaTime > 0.1f) deltaTime = 0.1f;

        platform_pump_messages(&platform);
        if (platform_should_close(&platform)) break;

        update(deltaTime);
        render(deltaTime);
    }
}

// ---- Callbacks ----
void Engine::set_render_callback(void (*callback)(void*), void* userData)
{
    renderCallback = callback;
    callbackData = userData;
}

void Engine::on_resize(int width, int height)
{
    if (m_settings.fullscreen) {
        return;
    }
    platform.width = width;
    platform.height = height;
    m_windowWidth = width;
    m_windowHeight = height;
    renderer.resize(width, height);
}