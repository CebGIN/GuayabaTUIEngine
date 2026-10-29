#include "WindowsTerminal.hpp"

#ifdef _WIN32

#include <cstdio>
#include <iostream>

namespace guayaba {

// Convert our platform-neutral color ID (0-15) to Windows WORD foreground attribute
WORD WindowsTerminal::color_id_to_win_fg(uint16_t color_id) {
    // Windows foreground bits: bit0=BLUE, bit1=GREEN, bit2=RED, bit3=INTENSITY
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

Key WindowsTerminal::vk_to_key(int virtual_key) {
    if (virtual_key >= 'A' && virtual_key <= 'Z')
        return static_cast<Key>(static_cast<uint16_t>(Key::A) + (virtual_key - 'A'));
    if (virtual_key >= '0' && virtual_key <= '9')
        return static_cast<Key>(static_cast<uint16_t>(Key::DIGIT_0) + (virtual_key - '0'));

    switch (virtual_key) {
        case VK_SPACE: return Key::SPACE;
        case VK_ESCAPE: return Key::ESCAPE;
        case VK_TAB: return Key::TAB;
        case VK_BACK: return Key::BACKSPACE;
        case VK_RETURN: return Key::ENTER;
        case VK_LEFT: return Key::LEFT;
        case VK_RIGHT: return Key::RIGHT;
        case VK_UP: return Key::UP;
        case VK_DOWN: return Key::DOWN;
        case VK_HOME: return Key::HOME;
        case VK_END: return Key::END;
        case VK_PRIOR: return Key::PAGE_UP;
        case VK_NEXT: return Key::PAGE_DOWN;
        case VK_INSERT: return Key::INSERT;
        case VK_DELETE: return Key::DELETE;
        case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: return Key::SHIFT;
        case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: return Key::CONTROL;
        case VK_MENU: case VK_LMENU: case VK_RMENU: return Key::ALT;
        case VK_LWIN: case VK_RWIN: return Key::SUPER;
        case VK_F1: return Key::F1;
        case VK_F2: return Key::F2;
        case VK_F3: return Key::F3;
        case VK_F4: return Key::F4;
        case VK_F5: return Key::F5;
        case VK_F6: return Key::F6;
        case VK_F7: return Key::F7;
        case VK_F8: return Key::F8;
        case VK_F9: return Key::F9;
        case VK_F10: return Key::F10;
        case VK_F11: return Key::F11;
        case VK_F12: return Key::F12;
        default: return Key::UNKNOWN;
    }
}

void WindowsTerminal::emit_key(Key key, bool pressed, char32_t text, bool ctrl,
                               bool shift, bool alt) {
    InputEvent event;
    event.type = InputEvent::Type::KEY;
    event.key = key;
    event.pressed = pressed;
    event.text = text;
    event.ctrl = ctrl;
    event.shift = shift;
    event.alt = alt;
    event.mouse_pos = mouse_pos;
    input_events.push_back(event);

    if (pressed && key != Key::NONE) keys_pressed.push_back(key);
}

void WindowsTerminal::poll_events() {
    l_click_prev = l_click;
    r_click_prev = r_click;

    l_click = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    r_click = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

    l_click_just_pressed = (l_click && !l_click_prev);
    r_click_just_pressed = (r_click && !r_click_prev);

    keys_pressed.clear();
    wheel_delta = 0;

    const int EVENT_COUNT = 128;
    INPUT_RECORD input_buffer[EVENT_COUNT];
    DWORD events_read = 0;
    DWORD events_available = 0;

    GetNumberOfConsoleInputEvents(hStdin, &events_available);
    if (events_available == 0) return;

    if (!ReadConsoleInput(hStdin, input_buffer, EVENT_COUNT, &events_read)) return;

    for (DWORD i = 0; i < events_read; ++i) {
        INPUT_RECORD& rec = input_buffer[i];

        if (rec.EventType == KEY_EVENT) {
            const KEY_EVENT_RECORD& key_rec = rec.Event.KeyEvent;

            // The console reports a key once per character typed, so a repeat
            // arrives as repeated presses; dropping them would make holding a
            // key type once.
            if (key_rec.wRepeatCount > 0) {
                for (WORD r = 0; r < key_rec.wRepeatCount; ++r) {
                    emit_key(
                        vk_to_key(key_rec.wVirtualKeyCode),
                        key_rec.bKeyDown != FALSE,
                        static_cast<char32_t>(key_rec.uChar.UnicodeChar),
                        (key_rec.dwControlKeyState &
                         (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0,
                        (key_rec.dwControlKeyState & SHIFT_PRESSED) != 0,
                        (key_rec.dwControlKeyState &
                         (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0);
                }
            }
        } else if (rec.EventType == MOUSE_EVENT) {
            const MOUSE_EVENT_RECORD& mouse_rec = rec.Event.MouseEvent;
            mouse_pos = {mouse_rec.dwMousePosition.X, mouse_rec.dwMousePosition.Y};

            if (mouse_rec.dwEventFlags == MOUSE_WHEELED) {
                // Signed, and in 120ths of a notch. Positive is up.
                const int notches = GET_WHEEL_DELTA_WPARAM(mouse_rec.dwButtonState) / 120;
                if (notches != 0) {
                    wheel_delta += notches;
                    InputEvent event;
                    event.type = InputEvent::Type::MOUSE_WHEEL;
                    event.wheel_delta = notches;
                    event.mouse_pos = mouse_pos;
                    input_events.push_back(event);
                }
            } else if (mouse_rec.dwEventFlags == MOUSE_MOVED) {
                InputEvent event;
                event.type = InputEvent::Type::MOUSE_MOTION;
                event.mouse_pos = mouse_pos;
                input_events.push_back(event);
            } else if (mouse_rec.dwEventFlags == 0) {
                // A button press or release. On a release the bit is already
                // cleared in dwButtonState, so which button cannot be told from
                // the record; the left button is reported, which is the common
                // case. Release still carries pressed=false, which a drag listens
                // for.
                InputEvent event;
                event.type = InputEvent::Type::MOUSE_BUTTON;
                event.mouse_pos = mouse_pos;
                if (mouse_rec.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) {
                    event.mouse_button = MouseButton::LEFT;
                    event.pressed = true;
                } else if (mouse_rec.dwButtonState & RIGHTMOST_BUTTON_PRESSED) {
                    event.mouse_button = MouseButton::RIGHT;
                    event.pressed = true;
                } else {
                    event.mouse_button = MouseButton::LEFT;
                    event.pressed = false;
                }
                input_events.push_back(event);
            }
        }
    }

    if (events_available > events_read) {
        FlushConsoleInputBuffer(hStdin);
    }
}

std::vector<InputEvent> WindowsTerminal::take_input_events() {
    std::vector<InputEvent> drained;
    drained.swap(input_events);
    return drained;
}

bool WindowsTerminal::was_key_pressed(Key key) const {
    for (Key pressed : keys_pressed) {
        if (pressed == key) return true;
    }
    return false;
}

int WindowsTerminal::get_wheel_delta() const { return wheel_delta; }

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