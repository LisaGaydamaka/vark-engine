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
#include "ui/ui_root.h"
#include "ui/ui_container.h"
#include "ui/ui_layout.h"
#include "ui/ui_label.h"
#include "ui/ui_panel.h"
#include "ui/ui_button.h"
#include "ui/ui_splitter.h"

HWND g_hwnd = nullptr;   // global, accessible to ui_splitter.cpp

static Renderer g_renderer;
static Level g_level;
static UIRenderer g_ui;
static Editor g_editor;
static std::unique_ptr<UIRoot> g_uiRoot;

static int g_clickX = 0, g_clickY = 0;
static bool g_clickValid = false;

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
            int modMask = 0;
            if (GetAsyncKeyState(VK_CONTROL) & 0x8000) modMask |= 1;
            if (GetAsyncKeyState(VK_SHIFT)   & 0x8000) modMask |= 2;
            if (GetAsyncKeyState(VK_MENU)    & 0x8000) modMask |= 4;
            if (leftDown || middleDown || rightDown) {
                g_editor.on_mouse_move(x - lastX, y - lastY, leftDown, middleDown, rightDown, modMask);
            }
            lastX = x;
            lastY = y;

            if (g_uiRoot) {
                g_uiRoot->on_mouse_move((float)x, (float)y);
            }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            g_clickX = x;
            g_clickY = y;
            g_clickValid = true;
            g_editor.on_mouse_button(0, true);
            if (g_uiRoot) {
                g_uiRoot->on_mouse_down((float)x, (float)y, 0);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            if (g_clickValid && abs(x - g_clickX) < 5 && abs(y - g_clickY) < 5) {
                bool consumed = g_editor.get_ui()->on_mouse_click(x, y);
                if (!consumed) {
                    int idx = g_editor.pick_brush(x, y);
                    if (idx >= 0) {
                        g_editor.select_brush(idx);
                    }
                }
            }
            g_clickValid = false;
            g_editor.on_mouse_button(0, false);
            if (g_uiRoot) {
                g_uiRoot->on_mouse_up((float)x, (float)y, 0);
            }
            return 0;
        }
        case WM_CHAR: {
            char c = (char)wParam;
            g_editor.get_ui()->on_text_input(c);
            if (g_uiRoot) {
                g_uiRoot->on_char(c);
            }
            return 0;
        }
        case WM_KEYDOWN: {
            bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            if (!g_editor.get_ui()->on_key_down((int)wParam)) {
                g_editor.on_key_down((int)wParam, ctrl, shift);
            }
            if (g_uiRoot) {
                g_uiRoot->on_key_down((int)wParam, ctrl, shift);
            }
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
    SetConsoleTitleA("Vark Editor");
    Logger::instance().init("logs/editor.log");
    LOG_INFO("=== Vark Editor ===");

    EditorSettings settings;
    settings.load("editor.ini");
    settings.save("editor.ini");

    int width = settings.display.windowWidth;
    int height = settings.display.windowHeight;

    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "VarkEditorClass";
    if (!RegisterClassA(&wc)) {
        LOG_ERROR("Failed to register window class");
        return -1;
    }

    g_hwnd = CreateWindowExA(0, "VarkEditorClass", "Vark Editor",
                             WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             width, height,
                             nullptr, nullptr, hInstance, nullptr);
    if (!g_hwnd) {
        LOG_ERROR("Failed to create window");
        return -1;
    }

    if (!g_renderer.initialize(g_hwnd, width, height)) {
        LOG_ERROR("Renderer init failed");
        return -1;
    }

    if (!g_ui.initialize(&g_renderer, width, height)) {
        LOG_ERROR("UI renderer init failed");
        return -1;
    }

    if (!g_editor.initialize(&g_renderer, &g_level, &g_ui)) {
        LOG_ERROR("Editor init failed");
        return -1;
    }
    g_editor.set_keybinds(settings.keybinds);

    const char* levelPath = "assets/levels/test.vmis";
    if (lpCmdLine && lpCmdLine[0]) {
        levelPath = lpCmdLine;
    }
    if (!g_level.build(&g_renderer, levelPath)) {
        LOG_WARN("Could not load level '%s', using empty level.", levelPath);
        g_level.clear();
    }
    g_editor.sync_brushes();

    // ---- Step 5: Splitter test with visible content ----
    g_uiRoot = std::make_unique<UIRoot>();

    auto splitter = std::make_unique<UISplitter>(UISplitter::Orientation::Vertical);
    splitter->set_rect(10.0f, 60.0f, 600.0f, 300.0f);
    splitter->set_ratio(0.4f);

    // Left panel
    auto leftPanel = std::make_unique<UIPanel>();
    leftPanel->set_background(0.3f, 0.1f, 0.1f, 1.0f);
    leftPanel->set_border(0.8f, 0.2f, 0.2f, 1.0f, 2.0f);

    auto leftLabel = std::make_unique<UILabel>("Left Panel", 1.0f, 1.0f, 1.0f, 1.0f);
    leftLabel->set_relative_rect(10.0f, 10.0f, 100.0f, 20.0f);  // relative to panel
    leftPanel->add_child(std::move(leftLabel));

    auto leftButton = std::make_unique<UIButton>("Click Left");
    leftButton->set_relative_rect(10.0f, 40.0f, 80.0f, 25.0f);
    leftButton->set_on_click([]() { LOG_INFO("Left button clicked!"); });
    leftPanel->add_child(std::move(leftButton));

    // Right panel
    auto rightPanel = std::make_unique<UIPanel>();
    rightPanel->set_background(0.1f, 0.1f, 0.3f, 1.0f);
    rightPanel->set_border(0.2f, 0.2f, 0.8f, 1.0f, 2.0f);

    auto rightLabel = std::make_unique<UILabel>("Right Panel", 1.0f, 1.0f, 1.0f, 1.0f);
    rightLabel->set_relative_rect(10.0f, 10.0f, 100.0f, 20.0f);
    rightPanel->add_child(std::move(rightLabel));

    auto rightButton = std::make_unique<UIButton>("Click Right");
    rightButton->set_relative_rect(10.0f, 40.0f, 80.0f, 25.0f);
    rightButton->set_on_click([]() { LOG_INFO("Right button clicked!"); });
    rightPanel->add_child(std::move(rightButton));

    splitter->add_child(std::move(leftPanel));
    splitter->add_child(std::move(rightPanel));
    g_uiRoot->add_child(std::move(splitter));

    MSG msg = {};

    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        g_editor.update(1.0f / 60.0f);

        g_renderer.begin_frame();

        int curWidth = g_ui.get_width();
        int curHeight = g_ui.get_height();

        Camera* cam = g_editor.get_camera();
        if (cam) {
            Mat4 view = cam->get_view_matrix();
            float aspect = (float)curWidth / (float)curHeight;
            Mat4 proj = mat4_perspective(60.0f * 3.14159265f / 180.0f, aspect, 0.1f, 1000.0f);
            Mat4 mvp = Mat4::identity() * view * proj;
            g_renderer.set_transform(mvp);
        }

        // ---- Disable 3D and old UI ----
        // g_level.render(&g_renderer);
        // g_editor.render();

        // ---- Render UI root ----
        if (g_uiRoot) {
            g_uiRoot->render_all(&g_ui);
        }

        g_renderer.end_frame();
    }

    g_editor.shutdown();
    g_ui.shutdown();
    g_level.shutdown(&g_renderer);
    g_renderer.shutdown();
    Logger::instance().shutdown();
    return 0;
}