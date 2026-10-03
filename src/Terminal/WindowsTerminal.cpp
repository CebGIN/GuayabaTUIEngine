#include "WindowsTerminal.hpp"

#ifdef _WIN32

#include <cstdio>
#include <iostream>

namespace guayaba {

// Convert our platform-neutral color ID (0-15) to Windows WORD foreground attribute
WORD WindowsTerminal::color_id_to_win_fg(uint16_t color_id) {
    // Windows foreground bits: bit0=BLUE, bit1=GREEN, bit2=RED, bit3=INTENSITY
    // Our IDs:   0=Black, 1=Red, 2=Green, 3=Yellow, 4=Blue, 5=Magenta, 6=Cyan, 7=White, 8-15=bright
    static const WORD table[16] = {
        0,                                                                  // 0  BLACK
        FOREGROUND_RED,                                                     // 1  RED
        FOREGROUND_GREEN,                                                   // 2  GREEN
        FOREGROUND_RED | FOREGROUND_GREEN,                                  // 3  YELLOW
        FOREGROUND_BLUE,                                                    // 4  BLUE
        FOREGROUND_RED | FOREGROUND_BLUE,                                   // 5  MAGENTA
        FOREGROUND_GREEN | FOREGROUND_BLUE,                                 // 6  CYAN
        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,                // 7  WHITE
        FOREGROUND_INTENSITY,                                               // 8  BRIGHT_BLACK
        FOREGROUND_RED | FOREGROUND_INTENSITY,                              // 9  BRIGHT_RED
        FOREGROUND_GREEN | FOREGROUND_INTENSITY,                            // 10 BRIGHT_GREEN
        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,           // 11 BRIGHT_YELLOW
        FOREGROUND_BLUE | FOREGROUND_INTENSITY,                             // 12 BRIGHT_BLUE
        FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY,            // 13 BRIGHT_MAGENTA
        FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,          // 14 BRIGHT_CYAN
        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY, // 15 BRIGHT_WHITE
    };
    return table[color_id & 0x0F];
}

WORD WindowsTerminal::color_id_to_win_bg(uint16_t color_id) {
    return color_id_to_win_fg(color_id) << 4;
}

void WindowsTerminal::init() {
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdin == INVALID_HANDLE_VALUE || hStdout == INVALID_HANDLE_VALUE) return;

    if (!GetConsoleMode(hStdin, &prev_mode)) return;
    DWORD new_mode = prev_mode;
    new_mode &= ~ENABLE_QUICK_EDIT_MODE;
    new_mode |= ENABLE_EXTENDED_FLAGS | ENABLE_MOUSE_INPUT;
    SetConsoleMode(hStdin, new_mode);

    CONSOLE_CURSOR_INFO cci;
    cci.dwSize = 1;
    cci.b_visible = FALSE;
    SetConsoleCursorInfo(hStdout, &cci);
}

void WindowsTerminal::restore() {
    if (hStdin != INVALID_HANDLE_VALUE) {
        SetConsoleMode(hStdin, prev_mode);
    }
    set_cursor_visible(true);
}

void WindowsTerminal::set_cursor_visible(bool visible) {
    if (hStdout == INVALID_HANDLE_VALUE) return;
    CONSOLE_CURSOR_INFO cci;
    cci.dwSize = 1;
    cci.b_visible = visible ? TRUE : FALSE;
    SetConsoleCursorInfo(hStdout, &cci);
}

void WindowsTerminal::set_cursor_position(Vec2 pos) {
    if (hStdout == INVALID_HANDLE_VALUE) return;
    COORD c = {static_cast<SHORT>(pos.x), static_cast<SHORT>(pos.y)};
    SetConsoleCursorPosition(hStdout, c);
}

Vec2 WindowsTerminal::get_screen_size(Vec2 fallback) const {
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(hStdout, &info)) {
        SHORT w = static_cast<SHORT>(info.srWindow.Right - info.srWindow.Left + 1);
        SHORT h = static_cast<SHORT>(info.srWindow.Bottom - info.srWindow.Top + 1);
        if (w > 0 && h > 0) {
            return {static_cast<int>(w), static_cast<int>(h)};
        }
    }
    return fallback;
}

void WindowsTerminal::present_buffer(const std::vector<TermChar>& buffer, Vec2 buffer_size) {
    if (hStdout == INVALID_HANDLE_VALUE) return;

    // Convert TermChar buffer to CHAR_INFO buffer
    std::vector<CHAR_INFO> win_buffer(buffer.size());
    for (size_t i = 0; i < buffer.size(); ++i) {
        win_buffer[i].Char.UnicodeChar = buffer[i].character;
        win_buffer[i].Attributes = color_id_to_win_fg(buffer[i].foreground) | color_id_to_win_bg(buffer[i].background);
    }

    COORD size = {static_cast<SHORT>(buffer_size.x), static_cast<SHORT>(buffer_size.y)};
    SMALL_RECT write_region = {0, 0, static_cast<SHORT>(buffer_size.x - 1), static_cast<SHORT>(buffer_size.y - 1)};

    if (!WriteConsoleOutputW(hStdout, win_buffer.data(), size, {0, 0}, &write_region)) {
        std::cerr << "Error writing to console output: " << GetLastError() << std::endl;
    }
}

void WindowsTerminal::poll_events() {
    l_click_prev = l_click;
    r_click_prev = r_click;

    l_click = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    r_click = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

    l_click_just_pressed = (l_click && !l_click_prev);
    r_click_just_pressed = (r_click && !r_click_prev);

    const int EVENT_COUNT = 128;
    INPUT_RECORD input_buffer[EVENT_COUNT];
    DWORD events_read = 0;
    DWORD events_available = 0;

    GetNumberOfConsoleInputEvents(hStdin, &events_available);
    if (events_available > 0) {
        if (!ReadConsoleInput(hStdin, input_buffer, EVENT_COUNT, &events_read)) return;

        for (DWORD i = 0; i < events_read; ++i) {
            INPUT_RECORD &rec = input_buffer[i];
            if (rec.EventType == MOUSE_EVENT) {
                MOUSE_EVENT_RECORD &mouse_rec = rec.Event.MouseEvent;
                if (mouse_rec.dwEventFlags == MOUSE_MOVED) {
                    mouse_pos = {mouse_rec.dwMousePosition.X, mouse_rec.dwMousePosition.Y};
                }
            }
        }
        if (events_available > events_read) {
            FlushConsoleInputBuffer(hStdin);
        }
    }
}

Vec2 WindowsTerminal::get_mouse_position() const { return mouse_pos; }
bool WindowsTerminal::is_left_click_pressed() const { return l_click; }
bool WindowsTerminal::is_right_click_pressed() const { return r_click; }
bool WindowsTerminal::is_left_click_just_pressed() const { return l_click_just_pressed; }
bool WindowsTerminal::is_right_click_just_pressed() const { return r_click_just_pressed; }

std::string WindowsTerminal::get_line_input(Vec2 position) {
    std::string input;
    set_cursor_position(position);
    set_cursor_visible(true);
    std::getline(std::cin, input);
    set_cursor_visible(false);
    return input;
}

} // namespace guayaba

#endif // _WIN32