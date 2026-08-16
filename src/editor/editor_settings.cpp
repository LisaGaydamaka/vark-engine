// src/editor/editor_settings.cpp
#include "editor_settings.h"
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <windows.h>

static void trim(std::string& s) {
    s.erase(0, s.find_first_not_of(" \t\r\n"));
    s.erase(s.find_last_not_of(" \t\r\n") + 1);
}

static void strip_comment(std::string& s) {
    size_t pos = s.find('#');
    if (pos != std::string::npos) s = s.substr(0, pos);
    trim(s);
}

EditorSettings::EditorSettings() {
    set_defaults();
}

void EditorSettings::set_defaults() {
    display.windowWidth = 1280;
    display.windowHeight = 720;
    display.fullscreen = false;

    keybinds.saveKey = 'S';
    keybinds.saveCtrl = true;
    keybinds.saveShift = false;

    keybinds.deleteKey = 0x2E;
    keybinds.deleteCtrl = false;
    keybinds.deleteShift = false;

    keybinds.orbitButton = MouseButton::Left;
    keybinds.orbitModifier = 0;

    keybinds.panButton = MouseButton::Middle;
    keybinds.panModifier = 0;
}

bool EditorKeybindSettings::matches_modifier(int vkModifier, int vkCtrl, int vkShift, int vkAlt) const {
    // vkModifier is the actual modifier state (bitmask: Ctrl=1, Shift=2, Alt=4)
    // our orbitModifier/panModifier is stored as the same bitmask.
    return (vkModifier & 0x7) == orbitModifier; // we only care about the lower bits
}

bool EditorSettings::load(const char* path) {
    set_defaults();

    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line, currentSection;
    while (std::getline(file, line)) {
        trim(line);
        if (line.empty() || line[0] == '#') continue;

        if (line[0] == '[') {
            size_t end = line.find(']');
            if (end != std::string::npos) {
                currentSection = line.substr(1, end - 1);
                trim(currentSection);
            }
            continue;
        }

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        trim(key);
        trim(val);
        strip_comment(val);

        if (currentSection == "Display") {
            if (key == "WindowWidth") display.windowWidth = std::stoi(val);
            else if (key == "WindowHeight") display.windowHeight = std::stoi(val);
            else if (key == "Fullscreen") display.fullscreen = (val == "true" || val == "1");
        }
        else if (currentSection == "Keybinds") {
            if (key == "Save") {
                parse_key_binding(val, keybinds.saveKey, keybinds.saveCtrl, keybinds.saveShift);
            }
            else if (key == "Delete") {
                parse_key_binding(val, keybinds.deleteKey, keybinds.deleteCtrl, keybinds.deleteShift);
            }
            else if (key == "Orbit") {
                parse_mouse_binding(val, keybinds.orbitButton, keybinds.orbitModifier);
            }
            else if (key == "Pan") {
                parse_mouse_binding(val, keybinds.panButton, keybinds.panModifier);
            }
        }
    }
    return true;
}

bool EditorSettings::save(const char* path) const {
    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << "# Editor Settings\n\n";
    file << "[Display]\n";
    file << "WindowWidth = " << display.windowWidth << "\n";
    file << "WindowHeight = " << display.windowHeight << "\n";
    file << "Fullscreen = " << (display.fullscreen ? "true" : "false") << "\n\n";

    file << "[Keybinds]\n";
    file << "# Keyboard commands: <Key> or <Modifier>+<Key> (e.g., Ctrl+S, Shift+Delete)\n";
    file << "Save = " << (keybinds.saveCtrl ? "Ctrl+" : "") << (keybinds.saveShift ? "Shift+" : "") << char(keybinds.saveKey) << "\n";
    file << "Delete = " << (keybinds.deleteCtrl ? "Ctrl+" : "") << (keybinds.deleteShift ? "Shift+" : "") << "Delete\n";

    file << "# Mouse navigation: <Modifier>+<Button> (e.g., Left, Shift+Middle, Ctrl+Right)\n";
    file << "# Buttons: Left, Middle, Right. Modifiers: Ctrl, Shift, Alt (or None)\n";
    std::string orbitStr;
    if (keybinds.orbitModifier & 1) orbitStr += "Ctrl+";
    if (keybinds.orbitModifier & 2) orbitStr += "Shift+";
    if (keybinds.orbitModifier & 4) orbitStr += "Alt+";
    switch (keybinds.orbitButton) {
        case MouseButton::Left:   orbitStr += "Left"; break;
        case MouseButton::Middle: orbitStr += "Middle"; break;
        case MouseButton::Right:  orbitStr += "Right"; break;
        default: orbitStr = "None";
    }
    file << "Orbit = " << orbitStr << "\n";

    std::string panStr;
    if (keybinds.panModifier & 1) panStr += "Ctrl+";
    if (keybinds.panModifier & 2) panStr += "Shift+";
    if (keybinds.panModifier & 4) panStr += "Alt+";
    switch (keybinds.panButton) {
        case MouseButton::Left:   panStr += "Left"; break;
        case MouseButton::Middle: panStr += "Middle"; break;
        case MouseButton::Right:  panStr += "Right"; break;
        default: panStr = "None";
    }
    file << "Pan = " << panStr << "\n";

    return true;
}

bool EditorSettings::parse_key_binding(const std::string& str, int& outVk, bool& outCtrl, bool& outShift) const {
    outCtrl = false; outShift = false;
    std::string s = str, keyName;
    size_t pos = s.find('+');
    if (pos != std::string::npos) {
        std::string mod = s.substr(0, pos);
        trim(mod);
        keyName = s.substr(pos + 1);
        trim(keyName);
        if (mod == "Ctrl") outCtrl = true;
        else if (mod == "Shift") outShift = true;
    } else {
        keyName = s;
        trim(keyName);
    }
    static std::unordered_map<std::string, int> keyMap = {
        {"Space", VK_SPACE}, {"Escape", VK_ESCAPE}, {"Delete", VK_DELETE},
        {"Insert", VK_INSERT}, {"Home", VK_HOME}, {"End", VK_END},
        {"PageUp", VK_PRIOR}, {"PageDown", VK_NEXT},
        {"F1", VK_F1}, {"F2", VK_F2}, {"F3", VK_F3}, {"F4", VK_F4},
        {"F5", VK_F5}, {"F6", VK_F6}, {"F7", VK_F7}, {"F8", VK_F8},
        {"F9", VK_F9}, {"F10", VK_F10}, {"F11", VK_F11}, {"F12", VK_F12},
        {"Up", VK_UP}, {"Down", VK_DOWN}, {"Left", VK_LEFT}, {"Right", VK_RIGHT}
    };
    auto it = keyMap.find(keyName);
    if (it != keyMap.end()) { outVk = it->second; return true; }
    if (keyName.length() == 1 && isalpha(keyName[0])) { outVk = toupper(keyName[0]); return true; }
    if (keyName.length() == 1) { outVk = keyName[0]; return true; }
    return false;
}

bool EditorSettings::parse_mouse_binding(const std::string& str, MouseButton& outButton, int& outModifier) const {
    outButton = MouseButton::None;
    outModifier = 0;
    std::string s = str, buttonName;
    size_t pos = s.find('+');
    if (pos != std::string::npos) {
        std::string mod = s.substr(0, pos);
        trim(mod);
        buttonName = s.substr(pos + 1);
        trim(buttonName);
        if (mod == "Ctrl") outModifier |= 1;
        else if (mod == "Shift") outModifier |= 2;
        else if (mod == "Alt") outModifier |= 4;
    } else {
        buttonName = s;
        trim(buttonName);
    }
    if (buttonName == "Left") outButton = MouseButton::Left;
    else if (buttonName == "Middle") outButton = MouseButton::Middle;
    else if (buttonName == "Right") outButton = MouseButton::Right;
    else if (buttonName == "None") outButton = MouseButton::None;
    else return false;
    return true;
}