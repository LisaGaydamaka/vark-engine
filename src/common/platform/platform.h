#pragma once

struct PlatformState
{
    void* windowHandle;
    int width;
    int height;
    bool shouldClose;
    void (*onResize)(int newWidth, int newHeight);
    int mouseWheelDelta;
    void* userData;
};

bool platform_create_window(PlatformState* state, int width, int height, const char* title, bool fullscreen);
void platform_pump_messages(PlatformState* state);
bool platform_should_close(const PlatformState* state);
void platform_destroy_window(PlatformState* state);
void platform_set_resize_callback(PlatformState* state, void (*callback)(int, int));