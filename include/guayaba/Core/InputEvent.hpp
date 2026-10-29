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
    One input occurrence. A control acting on an event removes it, so the same
    press cannot reach two controls.

    Press and release are separate events: a drag is press, motion, release, and
    `type` plus `pressed` says which half this is. A wheel notch is its own type,
    carrying its magnitude in wheel_delta.

    For a KEY event, text is the character the press produced as one codepoint
    ('a', 'A' with shift), or 0 for a key that types nothing. A field inserts text;
    a shortcut compares key. Modifier state is snapshotted per event.
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

    // The character a KEY press produced, or 0. char32_t: a codepoint is 32 bits
    // everywhere, whereas wchar_t is 2 on Windows.
    char32_t text = 0;

    // Only meaningful for MOUSE_BUTTON, MOUSE_MOTION and MOUSE_WHEEL.
    MouseButton mouse_button = MouseButton::NONE;
    Vec2 mouse_pos{};

    // How far the wheel turned, for MOUSE_WHEEL only. Positive is up; a magnitude,
    // since one report can carry several notches.
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
