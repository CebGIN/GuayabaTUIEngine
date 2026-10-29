#include <algorithm>
#include <utility>

#include <guayaba/Core/Color.hpp>
#include <guayaba/Core/Input.hpp>
#include <guayaba/Core/TextWrap.hpp>
#include <guayaba/Nodes/UI/Button.hpp>

namespace guayaba {

Button::Button(const std::string& node_name, Vec2 node_position)
    : Control(node_name, node_position) {
    // Declared here so connect_signal finds it.
    declare_signal("clicked");

    // Reachable by keyboard: clicking focuses it, and Tab reaches it.
    set_focus_mode(FocusMode::CLICK);
}

void Button::update(double delta_time) {
    // Hover is polled: the pointer leaving does not send this Button an event, so
    // the state must be read from Input. Activation is routed, in on_gui_input.
    hovered = is_inside(Input::mouse_pos);

    Control::update(delta_time);
}

bool Button::on_gui_input(const InputEvent& event) {
    if (disabled) return false;

    if (event.type == InputEvent::Type::MOUSE_BUTTON) {
        if (event.pressed && event.mouse_button == MouseButton::LEFT) {
            emit_signal("clicked");
            return true;
        }
    } else if (event.type == InputEvent::Type::KEY) {
        if (event.pressed && (event.key == Key::ENTER || event.key == Key::SPACE)) {
            emit_signal("clicked");
            return true;
        }
    }

    return false;
}

ButtonState Button::current_state() const {
    // Precedence, not iteration order: Disabled beats Pressed beats Hover.
    if (disabled) return ButtonState::Disabled;
    if (hovered && Input::l_click) return ButtonState::Pressed;
    if (hovered) return ButtonState::Hover;
    return ButtonState::Normal;
}

Rect Button::content_rect() const {
    const ButtonState state = current_state();

    // Fall back to the normal box when this state has none, so an unconfigured
    // hover does not move the text.
    const StyleBox& box = styleboxes[static_cast<int>(state)].has_background() ||
                                  styleboxes[static_cast<int>(state)].has_border()
                              ? styleboxes[static_cast<int>(state)]
                              : styleboxes[static_cast<int>(ButtonState::Normal)];

    return box.get_content_rect(get_rect());
}

Rect Button::text_clip(const Rect& incoming) const {
    if (!get_clip_children()) return incoming;

    // The text cannot paint over this Button's bottom border, but a too-wide line
    // spills sideways and is cut by the nearest clipping ancestor.
    const Rect content = content_rect();
    const Rect band{Vec2{incoming.left(), content.top()},
                    Vec2{incoming.size.x, content.size.y}};
    return incoming.intersection(band);
}

Vec2 Button::get_minimum_size() const {
    // Always on: a Button narrower than its label would clip the text it exists to
    // show.
    const Vec2 text = guayaba::measure_text(raw_text);

    // The normal box's frame, not the current state's, so a button does not resize
    // under a hovering cursor.
    const Vec2 frame = styleboxes[static_cast<int>(ButtonState::Normal)].get_frame_size();

    // The caller's own minimum still floors it.
    const Vec2 own = Control::get_minimum_size();
    return {std::max(own.x, text.x + frame.x), std::max(own.y, text.y + frame.y)};
}

void Button::set_text(const std::string& text) {
    raw_text = text;
    // Re-wrap now, so a set_text followed by a draw in the same frame shows the
    // new text.
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

    // Drawn exactly as large as it is, clipped only by its ancestors.
    if (box.has_background() || box.has_border()) {
        box.apply(renderer, area, clip);
    }

    const Rect content = content_rect();
    if (content.is_empty()) {
        Control::draw(renderer, clip);
        return;
    }

    // The text takes the background apply() just filled with, so a banded button
    // shows no holes.
    const uint16_t bg = box.has_background() ? *box.get_background_color() : 0;

    // Alignment slides the whole block sideways; the widest line decides, so every
    // line stays flush with the same edge. Text wider than the content is left
    // where it is.
    int text_x = content.pos.x;
    if (text_align != TextAlign::Left) {
        int widest = 0;
        for (const std::string& line : wrapped_lines) {
            widest = std::max(widest, static_cast<int>(line.size()));
        }
        const int room = content.size.x - widest;
        if (room > 0) {
            text_x += (text_align == TextAlign::Center) ? room / 2 : room;
        }
    }

    renderer.put_text({text_x, content.pos.y}, wrapped_lines,
                      text_colors[static_cast<int>(state)], bg, text_clip(clip));

    // Hand back to Control so the script hook runs and children use the clip
    // Control narrows for them.
    Control::draw(renderer, clip);
}

} // namespace guayaba