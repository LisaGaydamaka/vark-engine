// src/apps/editor/editor_main.cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <cstdio>
#include <cstdlib>
#include "renderer/renderer.h"
#include "world/level.h"
#include "ui/ui_renderer.h"
#include "core/logger.h"
#include "editor/editor.h"
#include "editor/editor_settings.h"

static HWND g_hwnd = nullptr;
static Renderer g_renderer;
static Level g_level;
static UIRenderer g_ui;
static Editor g_editor;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            if (width > 0 && height > 0) {
                g_renderer.resize(width, height);
                g_ui.resize(width, height);
            }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            g_editor.on_mouse_wheel(delta);
            return 0;
        }
        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            static int lastX = x, lastY = y;

            bool leftDown   = (wParam & MK_LBUTTON) != 0;
            bool middleDown = (wParam & MK_MBUTTON) != 0;
            bool rightDown  = (wParam & MK_RBUTTON) != 0;

            if (leftDown || middleDown || rightDown) {
                g_editor.on_mouse_move(x - lastX, y - lastY, leftDown, middleDown, rightDown);
            }

            lastX = x;
            lastY = y;
            return 0;
        }
        case WM_LBUTTONDOWN:
            g_editor.on_mouse_button(0, true);
            return 0;
        case WM_LBUTTONUP:
            g_editor.on_mouse_button(0, false);
            return 0;
        case WM_KEYDOWN: {
            bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            g_editor.on_key_down((int)wParam, ctrl, shift);
            return 0;
        }
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;

    AllocConsole();
    FILE* fDummy;
    freopen_s(&fDummy, "CONOUT$", "w", stdout);
    freopen_s(&fDummy, "CONOUT$", "w", stderr);
    SetConsoleTitleA("Vibe Editor");
    Logger::instance().init("logs/editor.log");
    LOG_INFO("=== Vibe Editor ===");

    // ---- Load editor settings ----
    EditorSettings settings;
    settings.load("editor.ini");
    settings.save("editor.ini"); // ensures file exists with defaults

    // ---- Create window ----
    int width = settings.display.windowWidth;
    int height = settings.display.windowHeight;

    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "VibeEditorClass";
    if (!RegisterClassA(&wc)) {
        LOG_ERROR("Failed to register window class");
        return -1;
    }

    g_hwnd = CreateWindowExA(0, "VibeEditorClass", "Vibe Editor",
                             WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             width, height,
                             nullptr, nullptr, hInstance, nullptr);
    if (!g_hwnd) {
        LOG_ERROR("Failed to create window");
        return -1;
    }

    // ---- Init renderer ----
    if (!g_renderer.initialize(g_hwnd, width, height)) {
        LOG_ERROR("Renderer init failed");
        return -1;
    }

    // ---- Init UI ----
    if (!g_ui.initialize(&g_renderer, width, height)) {
        LOG_ERROR("UI renderer init failed");
        return -1;
    }

    // ---- Init editor ----
    if (!g_editor.initialize(&g_renderer, &g_level, &g_ui)) {
        LOG_ERROR("Editor init failed");
        return -1;
    }
    g_editor.set_keybinds(settings.keybinds);

    // ---- Load level ----
    const char* levelPath = "assets/levels/test.vmis";
    if (lpCmdLine && lpCmdLine[0]) {
        levelPath = lpCmdLine;
    }
    if (!g_level.build(&g_renderer, levelPath)) {
        LOG_WARN("Could not load level '%s', using empty level.", levelPath);
        g_level.clear();
    }
    g_editor.sync_brushes();

    // ---- Main loop ----
    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        g_editor.update(1.0f / 60.0f);

        g_renderer.begin_frame();

        // Get current window dimensions from UI renderer (updated on resize)
        int width = g_ui.get_width();
        int height = g_ui.get_height();

        Camera* cam = g_editor.get_camera();
        if (cam) {
            Mat4 view = cam->get_view_matrix();
            float aspect = (float)width / (float)height;
            Mat4 proj = mat4_perspective(60.0f * 3.14159265f / 180.0f, aspect, 0.1f, 1000.0f);
            Mat4 mvp = Mat4::identity() * view * proj;
            g_renderer.set_transform(mvp);
        }

        g_level.render(&g_renderer);
        g_editor.render();

        g_ui.draw_text(10.0f, 10.0f, "Vibe Editor | Left=Orbit | Middle=Pan | Wheel=Zoom | Ctrl+S Save | Delete remove", 1.0f, 1.0f, 0.0f, 1.0f);

        g_renderer.end_frame();
    }

    // ---- Cleanup ----
    g_editor.shutdown();
    g_ui.shutdown();
    g_level.shutdown(&g_renderer);
    g_renderer.shutdown();
    Logger::instance().shutdown();
    return 0;
}