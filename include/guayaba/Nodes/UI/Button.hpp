#ifndef GUAYABA_NODES_UI_BUTTON_HPP
#define GUAYABA_NODES_UI_BUTTON_HPP

#include <cstdint>
#include <string>
#include <vector>

#include <guayaba/Core/Rect.hpp>
#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/UI/Control.hpp>
#include <guayaba/Renderer/ConsoleRenderer.hpp>
#include <guayaba/Resources/Stylebox.hpp>

namespace guayaba {

// The four looks a Button can be in, picked in this order of precedence: Disabled
// beats Pressed beats Hover. Normal is the resting state and the fallback for the
// others.
enum class ButtonState {
    Normal,
    Hover,
    Pressed,
    Disabled,
};

// Where the text sits horizontally inside the content rect. Left is the default.
// The lines of a wrapped label move together as one block.
enum class TextAlign {
    Left,
    Center,
    Right,
};

/*
    A Control that shows text inside a StyleBox and reacts to a left click.

        +---------------------------+
        |          Click me         |
        +---------------------------+

    Each state (ButtonState) carries its own StyleBox and text colour; draw picks
    the current one. The minimum size is the text's own extent plus the normal
    StyleBox's frame, measured from raw_text alone, and is always on.

    A "pressed" look means the left button is held while the pointer is over the
    Button; hover is read from Input in on_update(), since a Control only receives
    mouse events inside its rect. Activation - emitting "clicked" - happens in
    on_gui_input, so a press reaches exactly one control; Enter or Space while
    focused also activates. FocusMode defaults to CLICK.
*/
class Button : public Control {
protected:
    // One box and one text colour per state. Non-normal boxes start empty;
    // on_draw falls back to Normal.
    StyleBox styleboxes[4];
    uint16_t text_colors[4] = {7, 7, 7, 8}; // White; BRIGHT_BLACK when disabled

    std::string raw_text;
    std::vector<std::string> wrapped_lines;

    TextAlign text_align = TextAlign::Left;

    bool hovered = false;
    bool disabled = false;

    // The state draw() and the behaviour agree on.
    ButtonState current_state() const;

    // The current state's box minus its border and margins; empty when too small.
    Rect content_rect() const;

    // The area the text is drawn into: the current state's content rect when the
    // Button clips. Horizontal overflow is left to an ancestor, vertical is cut at
    // the Button's bottom.
    Rect text_clip(const Rect& incoming) const;

    // Activation: a left press inside the Button, or Enter/Space while focused.
    bool on_gui_input(const InputEvent& event) override;

public:
    explicit Button(const std::string& node_name = "Unnamed Button",
                    Vec2 node_position = {0, 0});

    void on_update(double delta_time) override;

    void on_draw(ConsoleRenderer& renderer, const Rect& clip) override;

    Vec2 get_minimum_size() const override;

    // --- State ---------------------------------------------------------------

    bool is_hovered() const { return hovered; }

    // True while the left button is held over this one.
    bool is_pressed() const { return !disabled && hovered && Input::l_click; }

    bool is_disabled() const { return disabled; }

    // A disabled Button ignores clicks and always draws as Disabled. Hover is
    // still tracked underneath.
    void set_disabled(bool is_disabled) { disabled = is_disabled; }

    // --- Text ---------------------------------------------------------------

    // Re-wraps against the current content width immediately.
    void set_text(const std::string& text);

    const std::string& get_text() const { return raw_text; }

    // The lines as last wrapped. Derived state; set raw_text through set_text().
    const std::vector<std::string>& get_wrapped_lines() const { return wrapped_lines; }

    // Where the text sits in the content rect. Center or Right only moves it; the
    // wrap and minimum size are unchanged.
    TextAlign get_text_align() const { return text_align; }
    void set_text_align(TextAlign alignment) { text_align = alignment; }

    // --- Appearance ---------------------------------------------------------

    StyleBox& get_stylebox(ButtonState state) { return styleboxes[static_cast<int>(state)]; }
    const StyleBox& get_stylebox(ButtonState state) const {
        return styleboxes[static_cast<int>(state)];
    }

    // A box change moves the content rect, so the wrap and cached minimum are recomputed.
    void set_stylebox(ButtonState state, StyleBox new_stylebox);

    uint16_t get_text_color(ButtonState state) const { return text_colors[static_cast<int>(state)]; }

    void set_text_color(ButtonState state, uint16_t color) {
        text_colors[static_cast<int>(state)] = color;
    }
    void set_text_color(ButtonState state, const std::string& color_name) {
        text_colors[static_cast<int>(state)] = Color::from_name(color_name);
    }
};

} // namespace guayaba

#endif // GUAYABA_NODES_UI_BUTTON_HPP