#ifndef GUAYABA_TERMINAL_LINUX_TERMINAL_HPP
#define GUAYABA_TERMINAL_LINUX_TERMINAL_HPP

// Private backend header. NOT part of the public API: it pulls in <termios.h>
// and must never be reachable from include/guayaba/.

#if defined(__linux__) || defined(__APPLE__)

#include <termios.h>

#include <cstdint>
#include <string>
#include <vector>

#include <guayaba/Core/InputEvent.hpp>
#include <guayaba/Core/Key.hpp>
#include <guayaba/Core/TermChar.hpp>
#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Terminal/IOSTerminal.hpp>

namespace guayaba {

class LinuxTerminal : public IOSTerminal {
private:
    struct termios original_termios;
    bool termios_stored = false;

    Vec2 mouse_pos = {0, 0};
    bool l_click = false;
    bool r_click = false;
    bool l_click_prev = false;
    bool r_click_prev = false;
    bool l_click_just_pressed = false;
    bool r_click_just_pressed = false;

    int wheel_delta = 0;

    /*
        Raw bytes read but not yet turned into events. A read can split a
        sequence arbitrarily - an arrow key's three bytes landing in two reads, a
        paste of thirty keystrokes - so a partial tail is held here and completed
        next poll instead of being guessed at.
    */
    std::string pending_bytes;

    // Whether a lone ESC has already waited one poll. A trailing ESC is
    // ambiguous: Escape pressed, or the first byte of a sequence yet to arrive.
    // After one empty poll it is taken as the Escape key.
    bool esc_waiting = false;

    // What the terminal sent since the last drain, and the set of keys that went
    // down this poll (kept apart so was_key_pressed survives the drain).
    std::vector<InputEvent> input_events;
    std::vector<Key> keys_pressed;

    // Modifiers as last reported by a named sequence. A terminal reports these
    // only alongside a cursor or function key, never on their own.
    bool shift_held = false;
    bool ctrl_held = false;
    bool alt_held = false;

    // Map color ID (0-15) to ANSI foreground code (30-37, 90-97)
    static int color_id_to_ansi_fg(uint16_t id);

    // Map color ID (0-15) to ANSI background code (40-47, 100-107)
    static int color_id_to_ansi_bg(uint16_t id);

    void enable_raw_mode();
    void disable_raw_mode();

    // Enable xterm mouse tracking (all movement + button events)
    void enable_mouse_tracking();
    void disable_mouse_tracking();

    // Parse SGR mouse event: \x1b[<button;x;y{M|m}
    bool parse_sgr_mouse(const char* buf, int len);

    // Turns pending_bytes into events, consuming what it recognises. Returns how
    // many leading bytes were used; an incomplete tail is left pending.
    std::size_t consume_input_bytes();

    // Pushes a key event and records the key as pressed this poll.
    void emit_key(Key key, bool pressed, char32_t text, bool ctrl, bool shift, bool alt);

    void emit_mouse_button(MouseButton button, bool pressed);
    void emit_mouse_motion();

public:
    void init() override;
    void restore() override;

    void set_cursor_visible(bool visible) override;
    void set_cursor_position(Vec2 pos) override;
    void present_buffer(const std::vector<TermChar>& buffer, Vec2 buffer_size) override;

    Vec2 get_screen_size(Vec2 fallback) const override;

    void poll_events() override;
    Vec2 get_mouse_position() const override;
    bool is_left_click_pressed() const override;
    bool is_right_click_pressed() const override;
    bool is_left_click_just_pressed() const override;
    bool is_right_click_just_pressed() const override;

    std::vector<InputEvent> take_input_events() override;
    bool was_key_pressed(Key key) const override;
    int get_wheel_delta() const override;

    std::string get_line_input(Vec2 position) override;
};

} // namespace guayaba

#endif // __linux__ || __APPLE__

#endif // GUAYABA_TERMINAL_LINUX_TERMINAL_HPP
