#include <algorithm>
#include <limits>

#include <guayaba/Core/Color.hpp>
#include <guayaba/Core/TextWrap.hpp>
#include <guayaba/Nodes/UI/Label.hpp>
#include <guayaba/Resources/Theme.hpp>

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

    // Off: no minimum of its own; the Label wraps into whatever size it is given.
    if (!fit_content) return own;

    // On: the text's extent plus the frame, so the border and margins come off the
    // text. The caller's floor still applies.
    const Vec2 text = measure_text();
    const Vec2 frame = stylebox.get_frame_size();

    return {std::max(own.x, text.x + frame.x), std::max(own.y, text.y + frame.y)};
}

Rect Label::content_rect() const { return stylebox.get_content_rect(get_rect()); }

Rect Label::text_clip(const Rect& incoming) const {
    if (!get_clip_children()) return incoming;

    // Vertical extent is the content rect, so overflow stops at the bottom border.
    // Horizontal stays incoming: a too-wide word spills and is cut by an ancestor.
    const Rect content = content_rect();
    const Rect vertical_band{Vec2{incoming.left(), content.top()},
                             Vec2{incoming.size.x, content.size.y}};
    return incoming.intersection(vertical_band);
}

void Label::on_update_layout(const Rect& parent_rect) {
    // Anchors first: the wrap needs the width they produce.
    Control::on_update_layout(parent_rect);

    // A zero or negative width means no wrapping, which a collapsed label wants,
    // so it goes straight to wrap.
    wrapped_lines = wrap(raw_text, content_rect().size.x);
}

void Label::set_text(const std::string& text) {
    raw_text = text;
    measured_size_valid = false;
    // Re-wrap now, so a set_text followed by a draw in the same frame shows the
    // new text.
    wrapped_lines = wrap(raw_text, content_rect().size.x);
}

void Label::set_stylebox(StyleBox new_stylebox) {
    stylebox = std::move(new_stylebox);
    // The frame is part of the minimum when fit_content is on, so the cache must go.
    measured_size_valid = false;
    wrapped_lines = wrap(raw_text, content_rect().size.x);
}

void Label::on_draw(ConsoleRenderer& renderer, const Rect& clip) {
    const Rect box = get_rect();

    // The box is clipped only by its ancestors, drawn exactly as large as it is.
    stylebox.apply(renderer, box, clip);

    const Rect content = content_rect();
    if (content.is_empty()) return;

    renderer.put_text(content.pos, wrapped_lines, text_color, background_color,
                      text_clip(clip));

    // Hand back to Control so the script hook runs and children use the clip
    // Control narrows for them.
    Control::on_draw(renderer, clip);
}

void Label::apply_theme(const Theme& theme) {
    stylebox = theme.get_style("label", stylebox);
    text_color = theme.get_color("text.primary", text_color);

    // The frame may have changed the content width; the wrap is recomputed on
    // the next layout pass.
    measured_size_valid = false;
    invalidate_cache_recursively();
}

} // namespace guayaba