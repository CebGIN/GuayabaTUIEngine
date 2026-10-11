#include <algorithm>

#include <guayaba/Resources/Stylebox.hpp>

namespace guayaba {

namespace {

// The thickest border that still leaves a cell of content on each side. A box
// one cell across has no interior to frame, so its border collapses to nothing
// and get_content_rect hands back the whole box, matching what the frame drew.
int max_border_width(const Rect& area) {
    return std::min(area.size.x, area.size.y) <= 1
               ? 0
               : (std::min(area.size.x, area.size.y) + 1) / 2;
}

int clamp_border_width(int width, const Rect& area) {
    return std::clamp(width, 0, max_border_width(area));
}

// The heavy style fills its interior with the shaded block so the frame reads
// as carved out of a solid slab.
wchar_t background_fill(StyleBorder style) {
    return style == StyleBorder::HEAVY ? L'\u2593' : L' ';
}

} // namespace

StyleBox::StyleBox() = default;

void StyleBox::apply(ConsoleRenderer& renderer, const Rect& area, const Rect& clip) const {
    if (area.is_empty()) return;

    // Fill first: the border sits on top of this, so the whole area is painted
    // before the frame goes down.
    if (background_color) {
        const uint16_t fg = border_color.value_or(7);  // White, the TermChar default
        renderer.fill_rect(area, background_fill(border_style), fg, *background_color, clip);
    }

    if (has_border()) {
        const uint16_t bg = background_color.value_or(0);  // Black, the TermChar default
        renderer.draw_box(area, border_style, *border_color, bg,
                          clamp_border_width(border_width, area), corner_radius, clip);
    }
}

Rect StyleBox::get_content_rect(const Rect& area) const {
    if (area.is_empty()) return Rect{area.pos, Vec2{0, 0}};

    const int width = clamp_border_width(border_width, area);
    const int pos_x = area.left() + width + content_margin.left;
    const int pos_y = area.top() + width + content_margin.top;

    // Rect's constructor floors a negative size at zero, which is exactly the
    // "too small to hold anything" case the callers are told to skip.
    return Rect{Vec2{pos_x, pos_y},
                Vec2{area.size.x - width * 2 - content_margin.horizontal(),
                     area.size.y - width * 2 - content_margin.vertical()}};
}

} // namespace guayaba