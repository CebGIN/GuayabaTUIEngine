#include <algorithm>
#include <utility>

#include <guayaba/Core/Color.hpp>
#include <guayaba/Core/Input.hpp>
#include <guayaba/Core/TextWrap.hpp>
#include <guayaba/Nodes/UI/Button.hpp>

namespace guayaba {

Button::Button(const std::string& node_name, Vec2 node_position)
    : Control(node_name, node_position) {
    // Declared here rather than on demand at the emit, so connect_signal finds
    // it and so is the one signal a Button is known to have.
    declare_signal("clicked");
}

void Button::update(double delta_time) {
    // Tested against the rect resolved last frame, which is also the rect this
    // frame draws: layout runs between update() and draw(), so reading the
    // freshest one here would mean hit-testing a size the user has not seen yet.
    hovered = is_inside(Input::mouse_pos);

    if (!disabled && hovered && Input::l_click_just_pressed) {
        emit_signal("clicked");
    }

    Control::update(delta_time);
}

ButtonState Button::current_state() const {
    // Precedence, not iteration order: a disabled Button stays visibly disabled
    // even while the pointer is over it and held.
    if (disabled) return ButtonState::Disabled;
    if (hovered && Input::l_click) return ButtonState::Pressed;
    if (hovered) return ButtonState::Hover;
    return ButtonState::Normal;
}

Rect Button::content_rect() const {
    const ButtonState state = current_state();

    // Fall back to the normal box when this state has none of its own, so an
    // unconfigured hover does not make the text jump to a different place.
    const StyleBox& box = styleboxes[static_cast<int>(state)].has_background() ||
                                  styleboxes[static_cast<int>(state)].has_border()
                              ? styleboxes[static_cast<int>(state)]
                              : styleboxes[static_cast<int>(ButtonState::Normal)];

    return box.get_content_rect(get_rect());
}

Rect Button::text_clip(const Rect& incoming) const {
    if (!get_clip_children()) return incoming;

    // Same split Label makes: the text cannot paint over this Button's own bottom
    // border, but a too-wide line spills sideways and is cut by the nearest
    // clipping ancestor.
    const Rect content = content_rect();
    const Rect band{Vec2{incoming.left(), content.top()},
                    Vec2{incoming.size.x, content.size.y}};
    return incoming.intersection(band);
}

Vec2 Button::get_minimum_size() const {
    // Always on: a Button laid out narrower than its own label would clip the
    // text it exists to show, which is the one thing a Button cannot do.
    const Vec2 text = guayaba::measure_text(raw_text);

    // The normal box's frame, not the current state's: the minimum has to be the
    // same whatever the pointer is doing, or a button would resize under the
    // cursor as it hovered.
    const Vec2 frame = styleboxes[static_cast<int>(ButtonState::Normal)].get_frame_size();

    // The caller's own minimum still floors it - neither can shrink the other.
    const Vec2 own = Control::get_minimum_size();
    return {std::max(own.x, text.x + frame.x), std::max(own.y, text.y + frame.y)};
}

void Button::set_text(const std::string& text) {
    raw_text = text;
    // Re-wrap now rather than leaving it stale until the next layout pass, so a
    // set_text followed by a draw in the same frame shows the new text.
    wrapped_lines = wrap(raw_text, content_rect().size.x);
}

void Button::set_stylebox(ButtonState state, StyleBox new_stylebox) {
    styleboxes[static_cast<int>(state)] = std::move(new_stylebox);
    // The box decides the content rect, which decides the wrap and the minimum.
    wrapped_lines = wrap(raw_text, content_rect().size.x);
}

void Button::draw(ConsoleRenderer& renderer, const Rect& clip) {
    const ButtonState state = current_state();
    const StyleBox& box = styleboxes[static_cast<int>(state)];
    const Rect area = get_rect();

    // The Button itself is drawn exactly as large as it is and is clipped only by
    // what its ancestors allowed: clip_children governs what this node contains,
    // not whether the node itself is visible.
    if (box.has_background() || box.has_border()) {
        box.apply(renderer, area, clip);
    }

    const Rect content = content_rect();
    if (content.is_empty()) {
        Control::draw(renderer, clip);
        return;
    }

    // The text takes the same background apply() just filled with, so a banded
    // button shows no holes; a state without one keeps the old transparent text.
    const uint16_t bg = box.has_background() ? *box.get_background_color() : 0;
    renderer.put_text(content.pos, wrapped_lines, text_colors[static_cast<int>(state)],
                      bg, text_clip(clip));

    // Hand back to Control so the script hook still runs and children are drawn
    // with the clip Control narrows for them.
    Control::draw(renderer, clip);
}

} // namespace guayaba