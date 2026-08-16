#include "platform.h"
#include <windows.h>
#include "engine/engine.h"

// --- Window Procedure ---
static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    PlatformState* state = (PlatformState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg)
    {
        case WM_DESTROY:
        {
            if (state) state->shouldClose = true;
            PostQuitMessage(0);
            return 0;
        }
        case WM_CLOSE:
        {
            DestroyWindow(hwnd);
            return 0;
        }
        case WM_SIZE:
        {
            if (state && state->userData)
            {
                int width = LOWORD(lParam);
                int height = HIWORD(lParam);
                if (width > 0 && height > 0)
                {
                    Engine* engine = (Engine*)state->userData;
                    engine->on_resize(width, height);
                }
            }
            return 0;
        }
        case WM_MOUSEWHEEL:
        {
            if (state) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                state->mouseWheelDelta += delta;
            }
            return 0;
        }
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

// --- Platform Implementation ---

bool platform_create_window(PlatformState* state, int width, int height, const char* title, bool fullscreen)
{
    state->shouldClose = false;
    state->onResize = nullptr;
    state->mouseWheelDelta = 0;
    state->userData = nullptr;

    HINSTANCE instance = GetModuleHandleA(nullptr);

    WNDCLASSA wc = {};
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.lpszClassName = "VarkEngineWindowClass";

    if (!RegisterClassA(&wc))
        return false;

    HWND hwnd;

    if (fullscreen)
    {
        // ---- Fullscreen: borderless window covering the entire monitor ----
        int screenWidth  = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);

        DWORD style = WS_POPUP;   // No title bar, no borders

        hwnd = CreateWindowExA(
            0,
            "VarkEngineWindowClass",
            title,
            style,
            0, 0,                     // top-left corner
            screenWidth, screenHeight,
            nullptr, nullptr,
            instance,
            nullptr
        );

        // Store the *requested* resolution (backbuffer size), not the screen size.
        state->width = width;
        state->height = height;
    }
    else
    {
        // ---- Windowed mode: fixed size, no resizing ----
        DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        RECT rect = { 0, 0, width, height };
        AdjustWindowRect(&rect, style, FALSE);
        int winWidth = rect.right - rect.left;
        int winHeight = rect.bottom - rect.top;

        hwnd = CreateWindowExA(
            0,
            "VarkEngineWindowClass",
            title,
            style,
            CW_USEDEFAULT, CW_USEDEFAULT,
            winWidth, winHeight,
            nullptr, nullptr,
            instance,
            nullptr
        );

        state->width = width;
        state->height = height;
    }

    if (!hwnd)
        return false;

    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
    state->windowHandle = (void*)hwnd;

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    return true;
}

// ---- The rest of the functions remain unchanged ----
void platform_pump_messages(PlatformState* state)
{
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
        {
            state->shouldClose = true;
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

bool platform_should_close(const PlatformState* state)
{
    return state->shouldClose;
}

void platform_destroy_window(PlatformState* state)
{
    if (state->windowHandle)
    {
        DestroyWindow((HWND)state->windowHandle);
        state->windowHandle = nullptr;
    }
}

void platform_set_resize_callback(PlatformState* state, void (*callback)(int, int))
{
    state->onResize = callback;
}

void platform_set_engine(Engine* engine)
{
    // Not used; we use state->userData.
}