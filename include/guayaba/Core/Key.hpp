#ifndef GUAYABA_CORE_KEY_HPP
#define GUAYABA_CORE_KEY_HPP

#include <cstdint>

namespace guayaba {

/*
    A physical key, not a character.

    The distinction is what a text field needs: the same key produces a different
    character depending on Shift, and a caller who wants the character asks for
    InputEvent::text rather than for the key. So KEY_A with shift held still reads
    A, and the 'a' that resulted is on the event as text.

    Letters, digits and space ARE their ASCII codes, so a key can be compared
    against a character literal:

        if (event.key == 'Q') quit();

    Special keys live above 0x100 so they cannot collide with any ASCII byte.

    Key::NONE is "no key", and is what an event carries when the type is not KEY.
    It is not the way to ask about any key - the event type answers that.
*/
enum class Key : uint16_t {
    NONE = 0,

    // ASCII-valued: the character a bare press produces.
    SPACE = 0x20,
    DIGIT_0 = 0x30, DIGIT_1, DIGIT_2, DIGIT_3, DIGIT_4,
    DIGIT_5, DIGIT_6, DIGIT_7, DIGIT_8, DIGIT_9,
    A = 0x41, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    // Everything below is a key with no character of its own, so it must not be
    // confused with an ASCII byte: 0x100 and up.
    ESCAPE = 0x100,
    TAB,
    BACKSPACE,
    ENTER,

    // Cursor movement. The four things a text field and a scrolling list both
    // need, so they are keys rather than escape sequences every caller sees.
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

    // Modifier keys. On Windows the console reports these as keys with a press
    // and a release; on Linux a terminal never reports a standalone Shift/Ctrl/
    // Alt, so these are delivered only where a sequence carries them. Prefer the
    // modifier flags on InputEvent, which is what the platform can answer.
    SHIFT,
    CONTROL,
    ALT,
    SUPER,

    // A key this build has no name for. Delivered rather than dropped, so a
    // terminal with an unrecognised key does not silently eat it.
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
