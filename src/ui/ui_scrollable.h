#pragma once

// Interface for widgets that can be scrolled vertically.
class IScrollable {
public:
    virtual ~IScrollable() = default;

    // Set the scroll offset (in pixels) – typically the top of the viewport.
    virtual void set_scroll_offset(float offset) = 0;

    // Total height of the content (used to compute scroll range).
    virtual float get_content_height() const = 0;

    // Optional: get content width (not used yet but can be added later).
    virtual float get_content_width() const { return 0.0f; }
};