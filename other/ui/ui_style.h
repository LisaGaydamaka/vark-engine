#pragma once

struct UIStyle {
    // ---------- Font ----------
    static constexpr float fontSize = 8.0f;
    static constexpr float fontWidth = 8.0f;   // monospace

    // ---------- Spacing ----------
    static constexpr float defaultSpacing = 4.0f;

    // ---------- Widget sizes ----------
    static constexpr float defaultItemHeight = 20.0f;
    static constexpr float defaultTextFieldHeight = 20.0f;
    static constexpr float defaultButtonHeight = 30.0f;
    static constexpr float scrollbarWidth = 16.0f;

    // ---------- Splitter ----------
    static constexpr float splitterHandleThickness = 4.0f;
    static constexpr float splitterHitThickness = 20.0f;

    // ---------- Text field padding ----------
    static constexpr float textFieldPaddingX = 4.0f;
    static constexpr float textFieldPaddingY = 2.0f;

    // ---------- Colours (RGBA) ----------

    // Panels
    static constexpr float panelBgR = 0.1f,   panelBgG = 0.1f,   panelBgB = 0.2f,   panelBgA = 1.0f;
    static constexpr float panelBorderR = 0.3f, panelBorderG = 0.3f, panelBorderB = 0.4f, panelBorderA = 1.0f;

    // Buttons
    static constexpr float buttonNormalR = 0.2f, buttonNormalG = 0.3f, buttonNormalB = 0.5f, buttonNormalA = 1.0f;
    static constexpr float buttonHoverR  = 0.3f, buttonHoverG  = 0.4f, buttonHoverB  = 0.6f, buttonHoverA  = 1.0f;
    static constexpr float buttonPressedR= 0.1f, buttonPressedG = 0.2f, buttonPressedB = 0.4f, buttonPressedA= 1.0f;
    static constexpr float buttonBorderLightR = 0.7f, buttonBorderLightG = 0.7f, buttonBorderLightB = 0.7f, buttonBorderLightA = 1.0f;
    static constexpr float buttonBorderDarkR  = 0.3f, buttonBorderDarkG  = 0.3f, buttonBorderDarkB  = 0.3f, buttonBorderDarkA  = 1.0f;
    static constexpr float buttonTextR = 1.0f, buttonTextG = 1.0f, buttonTextB = 1.0f, buttonTextA = 1.0f;

    // Text field
    static constexpr float textFieldBgR = 0.15f, textFieldBgG = 0.15f, textFieldBgB = 0.2f, textFieldBgA = 1.0f;
    static constexpr float textFieldBorderR = 0.3f, textFieldBorderG = 0.3f, textFieldBorderB = 0.4f, textFieldBorderA = 1.0f;
    static constexpr float textFieldTextR = 1.0f, textFieldTextG = 1.0f, textFieldTextB = 1.0f, textFieldTextA = 1.0f;
    static constexpr float textFieldPlaceholderR = 0.5f, textFieldPlaceholderG = 0.5f, textFieldPlaceholderB = 0.5f, textFieldPlaceholderA = 1.0f;
    static constexpr float textFieldSelectionR = 0.3f, textFieldSelectionG = 0.5f, textFieldSelectionB = 0.8f, textFieldSelectionA = 0.8f;

    // List
    static constexpr float listItemSelectedR = 0.3f, listItemSelectedG = 0.5f, listItemSelectedB = 0.8f, listItemSelectedA = 0.8f;
    static constexpr float listItemHoverR   = 0.3f, listItemHoverG   = 0.3f, listItemHoverB   = 0.3f, listItemHoverA   = 0.5f;
    static constexpr float listItemTextR    = 0.9f, listItemTextG    = 0.9f, listItemTextB    = 0.9f, listItemTextA    = 1.0f;
    static constexpr float listItemSelectedTextR = 1.0f, listItemSelectedTextG = 1.0f, listItemSelectedTextB = 0.8f, listItemSelectedTextA = 1.0f;
    static constexpr float listDragIndicatorR = 1.0f, listDragIndicatorG = 1.0f, listDragIndicatorB = 0.0f, listDragIndicatorA = 1.0f;

    // Scrollbar
    static constexpr float scrollbarTrackR = 0.2f, scrollbarTrackG = 0.2f, scrollbarTrackB = 0.25f, scrollbarTrackA = 1.0f;
    static constexpr float scrollbarThumbR = 0.5f, scrollbarThumbG = 0.5f, scrollbarThumbB = 0.6f, scrollbarThumbA = 1.0f;
    static constexpr float scrollbarThumbBorderLightR = 0.7f, scrollbarThumbBorderLightG = 0.7f, scrollbarThumbBorderLightB = 0.8f, scrollbarThumbBorderLightA = 1.0f;
    static constexpr float scrollbarThumbBorderDarkR  = 0.3f, scrollbarThumbBorderDarkG  = 0.3f, scrollbarThumbBorderDarkB  = 0.4f, scrollbarThumbBorderDarkA  = 1.0f;

    // Splitter handle
    static constexpr float splitterHandleR = 0.4f, splitterHandleG = 0.4f, splitterHandleB = 0.4f, splitterHandleA = 1.0f;
    static constexpr float splitterHandleBorderLightR = 0.5f, splitterHandleBorderLightG = 0.5f, splitterHandleBorderLightB = 0.5f, splitterHandleBorderLightA = 1.0f;
    static constexpr float splitterHandleBorderDarkR  = 0.3f, splitterHandleBorderDarkG  = 0.3f, splitterHandleBorderDarkB  = 0.3f, splitterHandleBorderDarkA  = 1.0f;

    // UI Root background (if any) – not used directly but could be.
    static constexpr float rootBgR = 0.0f, rootBgG = 0.0f, rootBgB = 0.0f, rootBgA = 0.0f; // transparent
};