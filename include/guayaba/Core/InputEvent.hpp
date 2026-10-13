#ifndef GUAYABA_CORE_INPUTEVENT_HPP
#define GUAYABA_CORE_INPUTEVENT_HPP

#include <cstdint>

#include <guayaba/Core/Key.hpp>
#include <guayaba/Core/Vec2.hpp>

namespace guayaba {

/*
    Mouse buttons, numbered the way the xterm SGR protocol numbers them, so the
    Linux backend's byte maps across without a table.
*/
enum class MouseButton : uint8_t {
    NONE = 0,
    LEFT = 1,
    RIGHT = 2,
    WHEEL_UP = 3,
    WHEEL_DOWN = 4,
};

/*
    One input occurrence.

    Events are the consumed view of input: a control acting on one removes it, so
    the same press cannot reach two controls. Input::mouse_pos and the l_click
    family stay as the polled view of the same thing, so code that only wants a
    state ("is the button down") keeps working.

    PRESS AND RELEASE ARE SEPARATE EVENTS
    A drag is press, then motion, then release, and a control tracking a drag
    needs all three. type plus pressed says which half this is.

    THE WHEEL IS ITS OWN TYPE
    A notch is not pointer movement, and reporting it as one would make every
    consumer inspect a button field to notice it. wheel_delta carries the
    magnitude so a fast scroll can be summed by the terminal.

    text IS NOT key
    For a KEY event, text is the character the press produced - 'a', 'A' with
    shift, an accented character from the layout - as one codepoint, or 0 for a
    key that types nothing (arrows, F-keys). A field inserts text; a shortcut
    compares key. Modifier state is snapshotted per event, so a user releasing
    Shift before the frame ends cannot change what Shift+A meant.
*/
struct InputEvent {
    enum class Type : uint8_t {
        KEY,           // a key went down or came up
        MOUSE_BUTTON,  // a button went down or came up
        MOUSE_MOTION,  // the pointer moved; pressed is always false
        MOUSE_WHEEL,   // a scroll notch arrived
    };

    Type type = Type::KEY;

    // Only meaningful for KEY.
    Key key = Key::NONE;

    // True for the press half of KEY and MOUSE_BUTTON.
    bool pressed = false;

    // Modifier state at the moment of the event.
    bool ctrl = false;
    bool shift = false;
    bool alt = false;

    // The character a KEY press produced, or 0. char32_t, not wchar_t: a Unicode
    // codepoint is 32 bits everywhere, whereas wchar_t is 2 on Windows.
    char32_t text = 0;

    // Only meaningful for MOUSE_BUTTON, MOUSE_MOTION and MOUSE_WHEEL.
    MouseButton mouse_button = MouseButton::NONE;
    Vec2 mouse_pos{};

    // How far the wheel turned, for MOUSE_WHEEL only. Positive is up. A
    // magnitude rather than a count, because a fast scroll sends several notches
    // in one report and the consumer wants "scroll twice as far".
    int wheel_delta = 0;

    bool is_key() const { return type == Type::KEY; }
    bool is_mouse_button() const { return type == Type::MOUSE_BUTTON; }
    bool is_mouse_motion() const { return type == Type::MOUSE_MOTION; }
    bool is_mouse_wheel() const { return type == Type::MOUSE_WHEEL; }

    bool is_pressed() const {
        return (type == Type::KEY || type == Type::MOUSE_BUTTON) && pressed;
    }

    bool is_released() const {
        return (type == Type::KEY || type == Type::MOUSE_BUTTON) && !pressed;
    }
};

} // namespace guayaba

#endif // GUAYABA_CORE_INPUTEVENT_HPP
