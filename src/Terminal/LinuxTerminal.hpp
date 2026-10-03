#ifndef GUAYABA_TERMINAL_LINUX_TERMINAL_HPP
#define GUAYABA_TERMINAL_LINUX_TERMINAL_HPP

// Private backend header. NOT part of the public API: it pulls in <termios.h>
// and must never be reachable from include/guayaba/.

#if defined(__linux__) || defined(__APPLE__)

#include <termios.h>

#include <cstdint>
#include <string>
#include <vector>

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

    std::string get_line_input(Vec2 position) override;
};

} // namespace guayaba

#endif // __linux__ || __APPLE__

#endif // GUAYABA_TERMINAL_LINUX_TERMINAL_HPP