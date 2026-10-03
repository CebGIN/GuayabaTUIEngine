#ifndef GUAYABA_TERMINAL_WINDOWSTERMINAL_HPP
#define GUAYABA_TERMINAL_WINDOWSTERMINAL_HPP

// Private backend header. NOT part of the public API: it pulls in <windows.h>
// and must never be reachable from include/guayaba/.

#ifdef _WIN32

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include <guayaba/Core/TermChar.hpp>
#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Terminal/IOSTerminal.hpp>

namespace guayaba {

class WindowsTerminal : public IOSTerminal {
private:
    HANDLE hStdin = INVALID_HANDLE_VALUE;
    HANDLE hStdout = INVALID_HANDLE_VALUE;
    DWORD prev_mode = 0;

    Vec2 mouse_pos = {0, 0};
    bool l_click = false;
    bool r_click = false;
    bool l_click_prev = false;
    bool r_click_prev = false;
    bool l_click_just_pressed = false;
    bool r_click_just_pressed = false;

    // Convert our platform-neutral color ID (0-15) to Windows WORD foreground attribute
    static WORD color_id_to_win_fg(uint16_t color_id);

    static WORD color_id_to_win_bg(uint16_t color_id);

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

#endif // _WIN32

#endif // GUAYABA_TERMINAL_WINDOWSTERMINAL_HPP