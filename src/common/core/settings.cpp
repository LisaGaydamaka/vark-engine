#include "settings.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
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
    size_t commentPos = s.find('#');
    if (commentPos != std::string::npos) {
        s = s.substr(0, commentPos);
    }
    trim(s);
}

// ---- Key name to virtual key code mapping ----
static std::unordered_map<std::string, int> build_key_map() {
    std::unordered_map<std::string, int> map;
    // Letters
    for (char c = 'A'; c <= 'Z'; ++c) {
        std::string s(1, c);
        map[s] = (int)c;
    }
    // Numbers
    for (char c = '0'; c <= '9'; ++c) {
        std::string s(1, c);
        map[s] = (int)c;
    }
    // Special keys
    map["SPACE"] = VK_SPACE;
    map["ESC"] = VK_ESCAPE;
    map["F1"] = VK_F1;
    map["F2"] = VK_F2;
    map["F3"] = VK_F3;
    map["F4"] = VK_F4;
    map["F5"] = VK_F5;
    map["F6"] = VK_F6;
    map["F7"] = VK_F7;
    map["F8"] = VK_F8;
    map["F9"] = VK_F9;
    map["F10"] = VK_F10;
    map["F11"] = VK_F11;
    map["F12"] = VK_F12;
    map["SHIFT"] = VK_SHIFT;
    map["CTRL"] = VK_CONTROL;
    map["ALT"] = VK_MENU;
    map["ENTER"] = VK_RETURN;
    map["TAB"] = VK_TAB;
    map["BACKSPACE"] = VK_BACK;
    map["DELETE"] = VK_DELETE;
    map["INSERT"] = VK_INSERT;
    map["HOME"] = VK_HOME;
    map["END"] = VK_END;
    map["PAGEUP"] = VK_PRIOR;
    map["PAGEDOWN"] = VK_NEXT;
    map["UP"] = VK_UP;
    map["DOWN"] = VK_DOWN;
    map["LEFT"] = VK_LEFT;
    map["RIGHT"] = VK_RIGHT;
    return map;
}

static int key_name_to_vk(const std::string& name) {
    static auto keyMap = build_key_map();
    auto it = keyMap.find(name);
    if (it != keyMap.end()) return it->second;
    // Fallback: try first character if single letter
    if (name.length() == 1 && isalpha(name[0])) {
        return (int)toupper(name[0]);
    }
    return 0; // unknown
}

static const char* action_to_name(Action action) {
    switch (action) {
        case Action::MoveForward:   return "MoveForward";
        case Action::MoveBackward:  return "MoveBackward";
        case Action::MoveLeft:      return "MoveLeft";
        case Action::MoveRight:     return "MoveRight";
        case Action::Jump:          return "Jump";
        case Action::Crouch:        return "Crouch";
        case Action::Run:           return "Run";
        case Action::Pause:         return "Pause";
        case Action::DebugToggle:   return "DebugToggle";
        default: return "";
    }
}

bool Settings::load(const char* filepath)
{
    // Set defaults first (including default keybinds)
    set_defaults();

    std::ifstream file(filepath);
    if (!file.is_open())
        return false;

    std::string line;
    std::string currentSection;
    while (std::getline(file, line))
    {
        trim(line);
        if (line.empty() || line[0] == '#')
            continue;

        if (line[0] == '[') {
            size_t end = line.find(']');
            if (end != std::string::npos) {
                currentSection = line.substr(1, end - 1);
                trim(currentSection);
            }
            continue;
        }

        size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        trim(key);
        trim(val);
        strip_comment(val);

        // ---- Display section ----
        if (currentSection == "Display" || currentSection.empty()) {
            if (key == "WindowWidth")           windowWidth   = std::stoi(val);
            else if (key == "WindowHeight")     windowHeight  = std::stoi(val);
            else if (key == "Fullscreen")       fullscreen    = (val == "true" || val == "1");
        }
        // ---- Camera section ----
        else if (currentSection == "Camera") {
            if (key == "FovDegrees")            fovDegrees    = std::stof(val);
            else if (key == "MouseSensitivity") mouseSensitivity = std::stof(val);
        }
        // ---- Level section ----
        else if (currentSection == "Level") {
            if (key == "LevelFile")             levelFile     = val;
        }
        // ---- Keybinds section ----
        else if (currentSection == "Keybinds") {
            // Map action name to key string (which may be a letter or special name)
            for (int i = 0; i < (int)Action::DebugToggle + 1; ++i) {
                Action act = (Action)i;
                const char* actName = action_to_name(act);
                if (key == actName) {
                    int vk = key_name_to_vk(val);
                    if (vk != 0) keybinds[act] = vk;
                    break;
                }
            }
        }
    }
    return true;
}

bool Settings::save(const char* filepath) const
{
    std::ofstream file(filepath);
    if (!file.is_open())
        return false;

    file << "# Vibe Engine Settings\n\n";
    file << "[Display]\n";
    file << "WindowWidth = " << windowWidth << "\n";
    file << "WindowHeight = " << windowHeight << "\n";
    file << "Fullscreen = " << (fullscreen ? "1" : "0") << "\n\n";
    file << "[Camera]\n";
    file << "FovDegrees = " << fovDegrees << "\n\n";
    file << "[Input]\n";
    file << "MouseSensitivity = " << mouseSensitivity << "\n\n";
    file << "[Level]\n";
    file << "LevelFile = " << levelFile << "\n\n";

    file << "[Keybinds]\n";
    file << "# Format: ActionName = KeyName (e.g., MoveForward = W)\n";
    file << "# Special keys: SPACE, ESC, F1-F12, SHIFT, CTRL, ALT, ENTER, TAB, etc.\n";
    file << "MoveForward = W\n";
    file << "MoveBackward = S\n";
    file << "MoveLeft = A\n";
    file << "MoveRight = D\n";
    file << "Jump = SPACE\n";
    file << "Crouch = C\n";
    file << "Run = SHIFT\n";
    file << "Pause = ESC\n";
    file << "DebugToggle = F3\n";

    return true;
}

void Settings::set_defaults()
{
    windowWidth = 1280;
    windowHeight = 720;
    fullscreen = false;
    fovDegrees = 60.0f;
    mouseSensitivity = 0.002f;
    levelFile = "assets/levels/test.vmis";

    // Default keybinds
    keybinds[Action::MoveForward] = 'W';
    keybinds[Action::MoveBackward] = 'S';
    keybinds[Action::MoveLeft] = 'A';
    keybinds[Action::MoveRight] = 'D';
    keybinds[Action::Jump] = VK_SPACE;
    keybinds[Action::Crouch] = 'C';
    keybinds[Action::Run] = VK_SHIFT;
    keybinds[Action::Pause] = VK_ESCAPE;
    keybinds[Action::DebugToggle] = VK_F3;
}

int Settings::get_key(Action action) const
{
    auto it = keybinds.find(action);
    if (it != keybinds.end()) return it->second;
    return 0; // unknown
}

bool Settings::is_key_down(Action action) const
{
    int vk = get_key(action);
    if (vk == 0) return false;
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}