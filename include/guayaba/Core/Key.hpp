#ifndef GUAYABA_CORE_KEY_HPP
#define GUAYABA_CORE_KEY_HPP

#include <cstdint>

namespace guayaba {

/*
    A physical key, not a character. The same key produces a different character
    with Shift held; the character is on InputEvent::text.

    Letters, digits and space are their ASCII codes, so a key compares against a
    character literal:

        if (event.key == 'Q') quit();

    Special keys live above 0x100, clear of any ASCII byte. Key::NONE means no
    key; ask about which keys an event carries through the event type.
*/
enum class Key : uint16_t {
    NONE = 0,

    // ASCII-valued: the character a bare press produces.
    SPACE = 0x20,
    DIGIT_0 = 0x30, DIGIT_1, DIGIT_2, DIGIT_3, DIGIT_4,
    DIGIT_5, DIGIT_6, DIGIT_7, DIGIT_8, DIGIT_9,
    A = 0x41, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    // Keys with no character of their own, above any ASCII byte.
    ESCAPE = 0x100,
    TAB,
    BACKSPACE,
    ENTER,

    // Cursor movement.
    LEFT,
    RIGHT,
    UP,
    DOWN,
    HOME,
    END,
    PAGE_UP,
    PAGE_DOWN,
    INSERT,
    DELETE,

    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    // Modifier keys. Windows reports these with a press and release; Linux only
    // where a sequence carries them. Prefer the modifier flags on InputEvent.
    SHIFT,
    CONTROL,
    ALT,
    SUPER,

    // A key this build has no name for; delivered rather than dropped.
    UNKNOWN,
};

// True for a key that produces a character on its own: letters, digits, space.
constexpr bool is_typing_key(Key key) {
    return (key >= Key::A && key <= Key::Z) ||
           (key >= Key::DIGIT_0 && key <= Key::DIGIT_9) ||
           key == Key::SPACE;
}

// True for a key that moves a caret or a selection without typing.
constexpr bool is_navigation_key(Key key) {
    return key == Key::LEFT || key == Key::RIGHT || key == Key::UP || key == Key::DOWN ||
           key == Key::HOME || key == Key::END || key == Key::PAGE_UP || key == Key::PAGE_DOWN;
}

} // namespace guayaba

#endif // GUAYABA_CORE_KEY_HPP
