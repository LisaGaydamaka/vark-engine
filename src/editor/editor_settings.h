// src/editor/editor_settings.h
#pragma once
#include <string>
#include <cstdint>

enum class MouseButton : int {
    None = 0,
    Left = 1,
    Middle = 2,
    Right = 3
};

struct EditorDisplaySettings {
    int windowWidth = 1280;
    int windowHeight = 720;
    bool fullscreen = false;
};

struct EditorKeybindSettings {
    // Keyboard commands
    int saveKey = 'S';
    bool saveCtrl = true;
    bool saveShift = false;

    int deleteKey = 0x2E;    // VK_DELETE
    bool deleteCtrl = false;
    bool deleteShift = false;

    // Mouse navigation
    MouseButton orbitButton = MouseButton::Left;
    int orbitModifier = 0;      // 0=None, 1=Ctrl, 2=Shift, 4=Alt

    MouseButton panButton = MouseButton::Middle;
    int panModifier = 0;

    // Helper: check if a modifier matches
    bool matches_modifier(int vkModifier, int vkCtrl, int vkShift, int vkAlt) const;
};

class EditorSettings {
public:
    EditorSettings();

    bool load(const char* path = "editor.ini");
    bool save(const char* path = "editor.ini") const;

    EditorDisplaySettings display;
    EditorKeybindSettings keybinds;

private:
    void set_defaults();

    // Parse "Ctrl+Left" style strings
    bool parse_mouse_binding(const std::string& str, MouseButton& outButton, int& outModifier) const;
    bool parse_key_binding(const std::string& str, int& outVk, bool& outCtrl, bool& outShift) const;
};