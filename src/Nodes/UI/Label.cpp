#include <algorithm>
#include <limits>

#include <guayaba/Core/Color.hpp>
#include <guayaba/Core/TextWrap.hpp>
#include <guayaba/Nodes/UI/Label.hpp>

namespace guayaba {

Label::Label(const std::string& node_name, Vec2 node_position)
    : Control(node_name, node_position) {}

Vec2 Label::measure_text() const {
    if (measured_size_valid) return measured_size;

    measured_size = guayaba::measure_text(raw_text);
    measured_size_valid = true;
    return measured_size;
}

Vec2 Label::get_minimum_size() const {
    const Vec2 own = Control::get_minimum_size();

    // Off means no minimum of its own - the caller imposed floor on its own, and
    // the Label wraps into whatever size it is given. That is what a Label in a
    // fixed frame wants.
    if (!fit_content) return own;

    // On means the text's own extent plus the frame drawn around it, so the
    // border and margins come off the text rather than landing on top of it.
    // The caller's floor still applies: neither can shrink the other.
    const Vec2 text = measure_text();
    const Vec2 frame = stylebox.get_frame_size();

    return {std::max(own.x, text.x + frame.x), std::max(own.y, text.y + frame.y)};
}

Rect Label::content_rect() const { return stylebox.get_content_rect(get_rect()); }

Rect Label::text_clip(const Rect& incoming) const {
    if (!get_clip_children()) return incoming;

    // Vertical extent is the content rect, so overflow stops at the inner edge
    // of the bottom border rather than painting over it. Horizontal extent
    // stays the incoming one: a too-wide word spills sideways and is cut by the
    // nearest clipping ancestor, while a too-tall block stops at the label.
    const Rect content = content_rect();
    const Rect vertical_band{Vec2{incoming.left(), content.top()},
                             Vec2{incoming.size.x, content.size.y}};
    return incoming.intersection(vertical_band);
}

void Label::update_layout(const Rect& parent_rect) {
    // Resolve anchors first: the wrap needs the width those produce, so it has
    // to run after Control has already placed us.
    Control::update_layout(parent_rect);

    // Wrapping against a zero or negative width would mean "no wrapping", which
    // is also what a collapsed label wants, so the degenerate case is handed
    // straight to wrap rather than special-cased here.
    wrapped_lines = wrap(raw_text, content_rect().size.x);
}

void Label::set_text(const std::string& text) {
    raw_text = text;
    measured_size_valid = false;
    // Re-wrap now instead of leaving the cache stale until the next layout pass,
    // so a set_text followed by a draw in the same frame shows the new text.
    wrapped_lines = wrap(raw_text, content_rect().size.x);
}

void Label::set_stylebox(StyleBox new_stylebox) {
    stylebox = std::move(new_stylebox);
    // The frame is part of the minimum size when fit_content is on, so a new
    // stylebox moves that floor too and the cache has to go.
    measured_size_valid = false;
    wrapped_lines = wrap(raw_text, content_rect().size.x);
}

void Label::draw(ConsoleRenderer& renderer, const Rect& clip) {
    const Rect box = get_rect();

    // The box itself is clipped only by what its ancestors allowed: it is drawn
    // exactly as large as it is, with no self-narrowing.
    stylebox.apply(renderer, box, clip);

    const Rect content = content_rect();
    if (content.is_empty()) return;

    renderer.put_text(content.pos, wrapped_lines, text_color, background_color,
                      text_clip(clip));

    // Hand back to Control so the script hook still runs and children are drawn
    // with the clip Control narrows for them.
    Control::draw(renderer, clip);
}

} // namespace guayaba