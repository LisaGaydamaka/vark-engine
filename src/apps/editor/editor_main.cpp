#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
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
#include "ui/ui_list.h"
#include "ui/ui_text_field.h"
#include "ui/ui_scroll_container.h"

static HWND g_hwnd = nullptr;
static Renderer g_renderer;
static Level g_level;
static UIRenderer g_ui;
static Editor g_editor;
static std::unique_ptr<UIRoot> g_uiRoot;

static int g_clickX = 0, g_clickY = 0;
static bool g_clickValid = false;
static int g_mouseX = 0, g_mouseY = 0;

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

            int screenX = GET_X_LPARAM(lParam);
            int screenY = GET_Y_LPARAM(lParam);
            POINT pt = { screenX, screenY };
            ScreenToClient(g_hwnd, &pt);

            if (g_uiRoot) {
                g_uiRoot->on_mouse_wheel((float)delta / 120.0f, (float)pt.x, (float)pt.y);
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            g_mouseX = x;
            g_mouseY = y;
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

    SetProcessDPIAware();

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

    if (!g_renderer.initialize(g_hwnd, width, height)) {
        LOG_ERROR("Renderer init failed");
        return -1;
    }

    if (!g_ui.initialize(&g_renderer, width, height)) {
        LOG_ERROR("UI renderer init failed");
        return -1;
    }

    RECT clientRect;
    GetClientRect(g_hwnd, &clientRect);
    int clientWidth = clientRect.right - clientRect.left;
    int clientHeight = clientRect.bottom - clientRect.top;
    if (clientWidth > 0 && clientHeight > 0) {
        g_renderer.resize(clientWidth, clientHeight);
        g_ui.resize(clientWidth, clientHeight);
        LOG_INFO("Adjusted to actual client size: %dx%d", clientWidth, clientHeight);
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

    // ---- Build UI ----
    LOG_INFO("Creating UI root and widgets...");
    g_uiRoot = std::make_unique<UIRoot>();

    auto splitter = std::make_unique<UISplitter>(UISplitter::Orientation::Vertical, 0.3f);
    splitter->set_rect(150.0f, 150.0f, 500.0f, 400.0f);
    splitter->set_hit_thickness(20.0f);

    auto leftContainer = std::make_unique<UIContainer>();
    leftContainer->set_layout(std::make_unique<UIFillLayout>());

    auto leftBg = std::make_unique<UIPanel>();
    leftBg->set_background(0.15f, 0.15f, 0.2f, 1.0f);
    leftBg->set_border(0.3f, 0.3f, 0.4f, 1.0f, 1.0f);

    // ---- Create list ----
    auto list = std::make_unique<UIList>();
    std::vector<std::pair<std::string, int>> brushItems;
    for (int i = 0; i < 200; ++i) {
        std::string label = "Brush " + std::to_string(i) + " (Item)";
        brushItems.push_back({label, i});
    }
    list->set_items(brushItems);
    list->set_on_selection_changed([](int idx) {
        LOG_INFO("UIList: selection changed to index %d", idx);
    });
    list->set_on_reordered([](const std::vector<int>& order) {
        std::string log = "UIList: new order: ";
        for (int idx : order) {
            log += std::to_string(idx) + " ";
        }
        LOG_INFO("%s", log.c_str());
    });

    // ---- Wrap list in scroll container ----
    auto scrollContainer = std::make_unique<UIScrollContainer>();
    scrollContainer->set_scrollbar_width(16.0f);
    scrollContainer->set_child(std::move(list));

    leftContainer->add_child(std::move(leftBg));
    leftContainer->add_child(std::move(scrollContainer));

    auto rightContainer = std::make_unique<UIContainer>();
    rightContainer->set_layout(std::make_unique<UIFillLayout>());

    auto rightBg = std::make_unique<UIPanel>();
    rightBg->set_background(0.1f, 0.1f, 0.15f, 1.0f);
    rightBg->set_border(0.3f, 0.3f, 0.4f, 1.0f, 1.0f);

    // ---- Right panel: VBox with padding and stretch ----
    auto vboxContainer = std::make_unique<UIContainer>();

    // Create VBox layout with padding
    auto vboxLayout = std::make_unique<UIVBoxLayout>(5.0f);
    vboxLayout->set_padding(8.0f, 8.0f, 8.0f, 8.0f);
    vboxContainer->set_layout(std::move(vboxLayout));

    // Label – fixed height
    auto label = std::make_unique<UILabel>("Value:", 0.8f, 0.8f, 0.8f, 1.0f);
    label->set_rect(0, 0, 80, 20);
    // No stretch – stays at preferred height
    vboxContainer->add_child(std::move(label));

    // Text field – will stretch
    auto textField = std::make_unique<UITextField>();
    textField->set_rect(0, 0, 120, 20);
    textField->set_placeholder("Enter value...");
    textField->set_commit_callback([](const std::string& val) {
        LOG_INFO("UITextField committed: %s", val.c_str());
    });
    textField->set_cancel_callback([]() {
        LOG_INFO("UITextField cancelled");
    });
    textField->set_stretch(1.0f);   // share extra space
    vboxContainer->add_child(std::move(textField));

    // Test button – also stretches
    auto testButton = std::make_unique<UIButton>("Test Click");
    testButton->set_rect(0, 0, 120, 30);
    testButton->set_on_click([]() {
        LOG_INFO(">>> Test button clicked! (callback fired)");
    });
    testButton->set_stretch(1.0f);
    vboxContainer->add_child(std::move(testButton));

    rightContainer->add_child(std::move(rightBg));
    rightContainer->add_child(std::move(vboxContainer));

    splitter->add_child(std::move(leftContainer));
    splitter->add_child(std::move(rightContainer));
    g_uiRoot->add_child(std::move(splitter));

    LOG_INFO("UI built, entering main loop.");

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

        // ---- Disable 3D for now ----
        // g_level.render(&g_renderer);
        // g_editor.render();

        // ---- Clipping test ----
        g_ui.draw_rect(0.0f, 0.0f, (float)curWidth, (float)curHeight, 1.0f, 0.0f, 0.0f, 0.5f);
        g_ui.push_clip_rect(50.0f, 50.0f, 100.0f, 100.0f);
        g_ui.draw_rect(0.0f, 0.0f, (float)curWidth, (float)curHeight, 0.0f, 1.0f, 0.0f, 0.7f);
        g_ui.pop_clip_rect();
        g_ui.draw_text(10.0f, 10.0f, "CLIP TEST: Red background, green clipped to (50,50)-(150,150)", 1.0f, 1.0f, 1.0f, 1.0f);

        // ---- Grid ----
        float gridColor[4] = {0.2f, 0.2f, 0.2f, 0.4f};
        for (int x = 0; x <= curWidth; x += 50) {
            g_ui.draw_rect((float)x, 0.0f, 1.0f, (float)curHeight, gridColor[0], gridColor[1], gridColor[2], gridColor[3]);
        }
        for (int y = 0; y <= curHeight; y += 50) {
            g_ui.draw_rect(0.0f, (float)y, (float)curWidth, 1.0f, gridColor[0], gridColor[1], gridColor[2], gridColor[3]);
        }

        // ---- Render UI ----
        if (g_uiRoot) {
            g_uiRoot->render_all(&g_ui);
        }

        // ---- Crosshair ----
        int cx = g_mouseX;
        int cy = g_mouseY;
        int len = 12;
        g_ui.draw_rect((float)(cx - len), (float)cy, (float)(len * 2), 1.0f, 0.0f, 1.0f, 0.0f, 1.0f);
        g_ui.draw_rect((float)cx, (float)(cy - len), 1.0f, (float)(len * 2), 0.0f, 1.0f, 0.0f, 1.0f);
        g_ui.draw_rect((float)(cx - 2), (float)(cy - 2), 5.0f, 5.0f, 0.0f, 1.0f, 0.0f, 1.0f);

        g_renderer.end_frame();
    }

    g_editor.shutdown();
    g_ui.shutdown();
    g_level.shutdown(&g_renderer);
    g_renderer.shutdown();
    Logger::instance().shutdown();
    return 0;
}