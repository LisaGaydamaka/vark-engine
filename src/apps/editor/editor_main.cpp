#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <cstdio>
#include "renderer/renderer.h"
#include "world/level.h"
#include "core/logger.h"
#include "editor/editor.h"
#include "editor/editor_settings.h"
#include "editor/editor_ui.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

static HWND g_hwnd = nullptr;
static Renderer g_renderer;
static Level g_level;
static Editor g_editor;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Track mouse position for Editor::on_mouse_move
static int g_lastMouseX = 0;
static int g_lastMouseY = 0;
static int g_mouseDownX = 0;
static int g_mouseDownY = 0;
static bool g_mouseDown = false;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return 0;

    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            if (width > 0 && height > 0) {
                g_renderer.resize(width, height);
            }
            return 0;
        }

        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            if (!ImGui::GetIO().WantCaptureMouse) {
                bool leftDown   = (wParam & MK_LBUTTON) != 0;
                bool middleDown = (wParam & MK_MBUTTON) != 0;
                bool rightDown  = (wParam & MK_RBUTTON) != 0;
                if (leftDown || middleDown || rightDown) {
                    int dx = x - g_lastMouseX;
                    int dy = y - g_lastMouseY;
                    int modMask = 0;
                    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) modMask |= 1;
                    if (GetAsyncKeyState(VK_SHIFT)   & 0x8000) modMask |= 2;
                    if (GetAsyncKeyState(VK_MENU)    & 0x8000) modMask |= 4;
                    g_editor.on_mouse_move(dx, dy, leftDown, middleDown, rightDown, modMask);
                }
            }
            g_lastMouseX = x;
            g_lastMouseY = y;
            return 0;
        }

        case WM_LBUTTONDOWN: {
            if (!ImGui::GetIO().WantCaptureMouse) {
                g_mouseDown = true;
                g_mouseDownX = GET_X_LPARAM(lParam);
                g_mouseDownY = GET_Y_LPARAM(lParam);
                SetFocus(hwnd);
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            if (!ImGui::GetIO().WantCaptureMouse && g_mouseDown) {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                int dx = x - g_mouseDownX;
                int dy = y - g_mouseDownY;
                if (dx * dx + dy * dy <= 9) {
                    LOG_INFO("WM_LBUTTONUP: click at (%d,%d), calling pick_brush", x, y);
                    int idx = g_editor.pick_brush(x, y);
                    LOG_INFO("WM_LBUTTONUP: pick_brush returned %d", idx);
                    if (idx >= 0) {
                        g_editor.select_brush(idx);
                    } else {
                        LOG_INFO("WM_LBUTTONUP: deselecting brush (click on empty space)");
                        g_editor.select_brush(-1);
                    }
                }
                g_mouseDown = false;
            }
            return 0;
        }

        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN: {
            if (!ImGui::GetIO().WantCaptureMouse) {
                SetFocus(hwnd);
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            if (!ImGui::GetIO().WantCaptureMouse) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                g_editor.on_mouse_wheel(delta);
            }
            return 0;
        }

        case WM_KEYDOWN: {
            if (!ImGui::GetIO().WantCaptureKeyboard) {
                bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                g_editor.on_key_down((int)wParam, ctrl, shift);
            }
            return 0;
        }

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;

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
    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);

    // ---- IMPORTANT: resize to the actual client area ----
    // The window's initial size (width,height) includes title bar and borders,
    // while the client area is smaller. This mismatch causes mouse coordinate offset.
    // Resizing after creation brings the renderer into sync with the client.
    RECT clientRect;
    GetClientRect(g_hwnd, &clientRect);
    int clientWidth = clientRect.right - clientRect.left;
    int clientHeight = clientRect.bottom - clientRect.top;
    if (clientWidth > 0 && clientHeight > 0) {
        g_renderer.resize(clientWidth, clientHeight);
        LOG_INFO("Adjusted to actual client size: %dx%d", clientWidth, clientHeight);
    }

    if (!g_renderer.initialize(g_hwnd, clientWidth, clientHeight)) {
        LOG_ERROR("Renderer init failed");
        return -1;
    }

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Optional: enable docking
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init((ID3D11Device*)g_renderer.get_device(),
                        (ID3D11DeviceContext*)g_renderer.get_context());

    if (!g_editor.initialize(&g_renderer, &g_level)) {
        LOG_ERROR("Editor init failed");
        return -1;
    }
    g_editor.set_keybinds(settings.keybinds);

    const char* levelPath = "assets/levels/test.vmis";
    if (!g_level.build(&g_renderer, levelPath)) {
        LOG_WARN("Could not load level '%s', using empty level.", levelPath);
        g_level.clear();
    }
    g_editor.sync_brushes();

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        // Start ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Build editor UI (menu bar + left/right panels)
        BuildEditorUI(g_editor);

        // ---- 3D rendering ----
        g_renderer.begin_frame();

        Camera* cam = g_editor.get_camera();
        if (cam) {
            Mat4 view = cam->get_view_matrix();
            int w = g_renderer.get_width();
            int h = g_renderer.get_height();
            float aspect = (float)w / (float)h;
            Mat4 proj = mat4_perspective(60.0f * 3.14159265f / 180.0f, aspect, 0.1f, 1000.0f);
            Mat4 mvp = Mat4::identity() * view * proj;
            g_renderer.set_transform(mvp);
        }

        g_level.render(&g_renderer);
        g_editor.render();

        // ---- Render ImGui ----
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_renderer.end_frame();
    }

    // Shutdown
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    g_editor.shutdown();
    g_level.shutdown(&g_renderer);
    g_renderer.shutdown();
    Logger::instance().shutdown();
    return 0;
}