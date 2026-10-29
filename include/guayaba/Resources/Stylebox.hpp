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
    Four insets in cells, all clamped to zero or more: the padding inside a border
    and the gap a control leaves for its children.
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

    // Renders this theme into area: the fill, then the frame on top. clip is the
    // region drawing is confined to; area is the box's own extent.
    void apply(ConsoleRenderer& renderer, const Rect& area, const Rect& clip) const;

    // The area left for content once the border and margins are taken off; empty
    // when the box is too small.
    Rect get_content_rect(const Rect& area) const;

    // The border on both sides plus both margins: the space to add to a content
    // size to get the box that holds it. The border is not clamped against an area,
    // so a box sized from this may have more room than needed but never clips.
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

    // An empty optional clears the color, falling back to whatever the terminal had.
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

    // Negative widths clamp to zero.
    void set_border_width(int width) { border_width = width < 0 ? 0 : width; }
    void set_border_style(StyleBorder style) { border_style = style; }

    // Also drops the width and radius, so switching back to a visible style does
    // not resurrect a stale thickness.
    void set_no_border() {
        border_style = StyleBorder::NONE;
        border_width = 0;
        corner_radius = 0;
    }

    void set_corner_radius(int radius) { corner_radius = radius < 0 ? 0 : radius; }

    // Each inset is floored at zero, matching EdgeSize's contract.
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