#pragma once
#include <cstdint>

struct UIStyle {
    // Colors
    static constexpr uint32_t COLOR_BG_DARK     = 0xFF181820;
    static constexpr uint32_t COLOR_BG_PANEL    = 0xFF20202C;
    static constexpr uint32_t COLOR_BG_SELECTED = 0xFF334466;
    static constexpr uint32_t COLOR_BORDER      = 0xFF444466;
    static constexpr uint32_t COLOR_TEXT        = 0xFFDDDDDD;
    static constexpr uint32_t COLOR_TEXT_SELECTED = 0xFFFFFFFF;
    static constexpr uint32_t COLOR_ACCENT      = 0xFF00FF00;
    static constexpr uint32_t COLOR_BUTTON      = 0xFF336633;
    static constexpr uint32_t COLOR_BUTTON_HOVER = 0xFF448844;
    static constexpr uint32_t COLOR_BUTTON_DELETE = 0xFF663333;

    // Metrics
    static constexpr int FONT_CHAR_WIDTH  = 8;
    static constexpr int FONT_CHAR_HEIGHT = 8;
    static constexpr int PANEL_PADDING    = 8;
    static constexpr int LINE_HEIGHT      = 20;
    static constexpr int TOP_MENU_HEIGHT  = 30;
    static constexpr int SPLITTER_WIDTH   = 6;
};