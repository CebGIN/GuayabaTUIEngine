#ifndef GUAYABA_UI_STYLEBOX_HPP
#define GUAYABA_UI_STYLEBOX_HPP

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include <guayaba/Core/Color.hpp>
#include <guayaba/Core/Rect.hpp>
#include <guayaba/Renderer/ConsoleRenderer.hpp>

namespace guayaba {

/*
    Four insets in cells, all clamped to zero or more. This is both the padding
    inside a border and the gap a control leaves for its children: a Panel with
    a content margin of 1 puts its title one column in from the frame.
*/ 
struct EdgeSize {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;

    int horizontal() const { return left + right; }
    int vertical() const { return top + bottom; }
};

class StyleBox {
private:
    std::optional<uint16_t> background_color;
    std::optional<uint16_t> border_color;
    int border_width = 0;
    StyleBorder border_style = StyleBorder::NONE;
    int corner_radius = 0;
    EdgeSize content_margin{};

public:

    StyleBox();

    // Renders this theme into area: the fill, then the frame on top of it. Both
    // go through the renderer's batch primitives, so this class holds no drawing
    // loops of its own and cannot disagree with the renderer about clipping.
    //
    // clip is the region drawing is confined to and area the box's own extent.
    // They are separate because the box may be larger than whatever contains it.
    void apply(ConsoleRenderer& renderer, const Rect& area, const Rect& clip) const;

    // The area left for content once the border and margins are taken off.
    // Empty when the box is too small to hold anything, which a caller should
    // treat as "draw nothing" rather than as a negative size to compensate for.
    Rect get_content_rect(const Rect& area) const;

    // What get_content_rect takes off: the border on both sides plus both
    // margins. This is the space a caller has to add to a content size to get
    // the box that will hold it.
    //
    // The border is not clamped the way get_content_rect clamps it, because
    // there is no area to clamp against yet. That errs towards reserving a
    // frame at most as thick as was asked for, so a box sized from this can
    // come out with more room than it needs but never clips its own content.
    Vec2 get_frame_size() const {
        return Vec2{content_margin.horizontal() + border_width * 2,
                    content_margin.vertical() + border_width * 2};
    }

    std::optional<uint16_t> get_background_color() const { return background_color; }
    std::optional<uint16_t> get_border_color() const { return border_color; }
    int get_border_width() const { return border_width; }
    StyleBorder get_border_style() const { return border_style; }
    int get_corner_radius() const { return corner_radius; }
    EdgeSize get_content_margin() const { return content_margin; }

    bool has_background() const { return background_color.has_value(); }
    bool has_border() const {
        return border_style != StyleBorder::NONE && border_color.has_value() && border_width > 0;
    }

    // Passing an empty optional clears the color, which is how you fall back to
    // whatever the terminal already had in those cells.
    void set_background_color(std::optional<uint16_t> color) { background_color = color; }
    void set_background_color(const std::string& color_name) {
        background_color = Color::from_name(color_name);
    }
    void clear_background_color() { background_color.reset(); }

    void set_border_color(std::optional<uint16_t> color) { border_color = color; }
    void set_border_color(const std::string& color_name) {
        border_color = Color::from_name(color_name);
    }
    void clear_border_color() { border_color.reset(); }

    // Negative widths are meaningless, so they clamp to zero rather than being
    // stored and then having to be defended against at every draw.
    void set_border_width(int width) { border_width = width < 0 ? 0 : width; }
    void set_border_style(StyleBorder style) { border_style = style; }

    // Setting the style back to NONE also drops the width and radius: has_border()
    // already ignores them, and leaving stale values around makes a later
    // switch back to a visible style resurrect a thickness nobody asked for.
    void set_no_border() {
        border_style = StyleBorder::NONE;
        border_width = 0;
        corner_radius = 0;
    }

    void set_corner_radius(int radius) { corner_radius = radius < 0 ? 0 : radius; }

    // Each inset is floored at zero, matching the "clamped to zero or more"
    // contract EdgeSize documents. A negative margin is a caller bug, and
    // storing it would grow the content rect past the box it came from.
    void set_content_margin(const EdgeSize& margin) {
        content_margin.left = margin.left < 0 ? 0 : margin.left;
        content_margin.top = margin.top < 0 ? 0 : margin.top;
        content_margin.right = margin.right < 0 ? 0 : margin.right;
        content_margin.bottom = margin.bottom < 0 ? 0 : margin.bottom;
    }

    void set_content_margin(int left, int top, int right, int bottom) {
        set_content_margin(EdgeSize{left, top, right, bottom});
    }

};

} // namespace guayaba

#endif // GUAYABA_UI_STYLEBOX_HPP