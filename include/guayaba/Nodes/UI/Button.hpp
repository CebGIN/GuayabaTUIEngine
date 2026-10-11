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

// The four looks a Button can be in. Exactly one is current at a time, picked in
// this order of precedence - Disabled beats Pressed beats Hover - so a disabled
// button cannot flash the pressed look just because the mouse happens to be
// held over it.
//
// Normal is the resting state and the only one a Button really needs: the others
// fall back to it, so a Button with three boxes left at their defaults still
// draws and still hovers.
enum class ButtonState {
    Normal,
    Hover,
    Pressed,
    Disabled,
};

/*
    A Control that shows text inside a StyleBox and reacts to a left click.

    It is a Label that is also clickable: same text handling, same wrapping, and
    a minimum size that always fits what it says.

        +---------------------------+
        |          Click me         |
        +---------------------------+

    Each state carries its own StyleBox and its own text color, so a hover can be
    a different background rather than a foreground/background swap. Draw picks
    the current one; there is no "theme" object, because nothing else in the
    engine has one yet and inventing it here would be a layer with a single user.

    SIZE
    The minimum is the text's own extent plus the normal StyleBox's frame, and it
    is always on - a Button with no minimum could be laid out narrower than its
    own label and clip the text it exists to show. As with every Control this is
    a floor, not a target: set_size(), FullRect and the anchors still decide how
    big the Button actually gets, and a button in a wide cell is drawn wide.

    Measured from raw_text alone, so the answer does not depend on the size the
    layout handed out. See TextWrap::measure_text.

    PRESSING
    "Pressed" means the left button is held while the pointer is over this
    Button, and "clicked" fires on the frame the press begins. Both are read from
    Input in update(), so a Button sees the mouse one frame before it is drawn
    against the layout that frame - the rect it tests is the one resolved last
    frame, which is the same rect it draws with.

    There is no keyboard activation and no focus, because the engine has no key
    input to build on. A Button is reachable by pointer only.

    "clicked" is a signal, declared in the constructor and the only way to say
    what a Button does. There is deliberately no set_on_click: two callbacks
    where only one can be installed is one callback too many.
*/
class Button : public Control {
protected:
    // One box and one text color per state. The three non-normal boxes start out
    // empty, and draw falls back to Normal when the current state has no box of
    // its own, so they cost nothing until they are configured.
    StyleBox styleboxes[4];
    uint16_t text_colors[4] = {7, 7, 7, 8}; // White; BRIGHT_BLACK when disabled

    std::string raw_text;
    std::vector<std::string> wrapped_lines;

    bool hovered = false;
    bool disabled = false;

    // The state draw() and the frame's behaviour should agree on, rather than
    // each picking its own precedence rule.
    ButtonState current_state() const;

    // Where the text goes: the current state's box, minus its border and
    // margins. Empty when the box is too small to hold anything.
    Rect content_rect() const;

    // The area the text is drawn into, narrowed to the current state's content
    // rect when the Button clips. Horizontal overflow is left to an ancestor and
    // vertical overflow is cut here, at the Button's own bottom.
    Rect text_clip(const Rect& incoming) const;

public:
    explicit Button(const std::string& node_name = "Unnamed Button",
                    Vec2 node_position = {0, 0});

    void update(double delta_time) override;

    void draw(ConsoleRenderer& renderer, const Rect& clip) override;

    Vec2 get_minimum_size() const override;

    // --- State ---------------------------------------------------------------

    bool is_hovered() const { return hovered; }

    // True while the left button is held over this one.
    bool is_pressed() const { return !disabled && hovered && Input::l_click; }

    bool is_disabled() const { return disabled; }

    // A disabled Button ignores clicks and always draws as Disabled. Hover state
    // is still tracked underneath so it comes back correct when re-enabled.
    void set_disabled(bool is_disabled) { disabled = is_disabled; }

    // --- Text ---------------------------------------------------------------

    // Re-wraps against the current content width, so a change here is visible
    // this frame without waiting for the next layout pass.
    void set_text(const std::string& text);

    const std::string& get_text() const { return raw_text; }

    // The lines as last wrapped. Derived state; set raw_text through set_text().
    const std::vector<std::string>& get_wrapped_lines() const { return wrapped_lines; }

    // --- Appearance ---------------------------------------------------------

    StyleBox& get_stylebox(ButtonState state) { return styleboxes[static_cast<int>(state)]; }
    const StyleBox& get_stylebox(ButtonState state) const {
        return styleboxes[static_cast<int>(state)];
    }

    // A box change can move the content rect, so the wrap and the cached minimum
    // are both recomputed rather than left stale.
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