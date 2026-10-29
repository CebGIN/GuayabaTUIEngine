#include "LinuxTerminal.hpp"

#if defined(__linux__) || defined(__APPLE__)

#include <unistd.h>
#include <sys/ioctl.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sstream>

#include "KeySequence.hpp"

namespace guayaba {

// Map color ID (0-15) to ANSI foreground code (30-37, 90-97)
int LinuxTerminal::color_id_to_ansi_fg(uint16_t id) {
    // 0-7 -> 30-37,  8-15 -> 90-97
    if (id < 8) return 30 + id;
    return 90 + (id - 8);
}

// Map color ID (0-15) to ANSI background code (40-47, 100-107)
int LinuxTerminal::color_id_to_ansi_bg(uint16_t id) {
    if (id < 8) return 40 + id;
    return 100 + (id - 8);
}

void LinuxTerminal::enable_raw_mode() {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_cc[VMIN] = 0;   // Non-blocking read
    raw.c_cc[VTIME] = 0;  // No timeout
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void LinuxTerminal::disable_raw_mode() {
    if (termios_stored) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
    }
}

// Enable xterm mouse tracking (all movement + button events)
void LinuxTerminal::enable_mouse_tracking() {
    // Enable button event tracking + SGR extended mode
    write(STDOUT_FILENO, "\x1b[?1003h", 8);  // All motion tracking
    write(STDOUT_FILENO, "\x1b[?1006h", 8);  // SGR extended mouse mode
}

void LinuxTerminal::disable_mouse_tracking() {
    write(STDOUT_FILENO, "\x1b[?1003l", 8);
    write(STDOUT_FILENO, "\x1b[?1006l", 8);
}

// Parse SGR mouse event: \x1b[<button;x;y{M|m}
bool LinuxTerminal::parse_sgr_mouse(const char* buf, int len) {
    // Looking for pattern: ESC [ < Cb ; Cx ; Cy M/m
    // Where M = press, m = release
    int i = 0;
    if (len < 6) return false;
    if (buf[0] != '\x1b' || buf[1] != '[' || buf[2] != '<') return false;
    i = 3;

    int button = 0, cx = 0, cy = 0;
    // Parse button
    while (i < len && buf[i] >= '0' && buf[i] <= '9') { button = button * 10 + (buf[i] - '0'); i++; }
    if (i >= len || buf[i] != ';') return false;
    i++;
    // Parse x
    while (i < len && buf[i] >= '0' && buf[i] <= '9') { cx = cx * 10 + (buf[i] - '0'); i++; }
    if (i >= len || buf[i] != ';') return false;
    i++;
    // Parse y
    while (i < len && buf[i] >= '0' && buf[i] <= '9') { cy = cy * 10 + (buf[i] - '0'); i++; }
    if (i >= len) return false;

    bool is_release = (buf[i] == 'm');

    // SGR coordinates are 1-based, convert to 0-based
    mouse_pos = {cx - 1, cy - 1};

    int base_button = button & 0x03; // Bottom 2 bits = button
    bool is_motion = (button & 32) != 0;
    // The wheel is bits 6/7 with no press/release distinction, so it cannot go
    // through the press/release path below.
    bool is_wheel = (button & 64) != 0;

    if (is_wheel) {
        // Bit 0 says which way: 64 is up, 65 is down. A notch is an occurrence,
        // so it gets its own event type.
        const int delta = (button & 1) != 0 ? -1 : 1;
        wheel_delta += delta;

        InputEvent event;
        event.type = InputEvent::Type::MOUSE_WHEEL;
        event.wheel_delta = delta;
        event.mouse_pos = mouse_pos;
        event.ctrl = ctrl_held;
        event.shift = shift_held;
        event.alt = alt_held;
        input_events.push_back(event);
        return true;
    }

    if (!is_motion) {
        if (base_button == 0) { // Left button
            l_click = !is_release;
            emit_mouse_button(MouseButton::LEFT, !is_release);
        } else if (base_button == 2) { // Right button
            r_click = !is_release;
            emit_mouse_button(MouseButton::RIGHT, !is_release);
        }
    } else {
        emit_mouse_motion();
    }

    return true;
}

void LinuxTerminal::emit_key(Key key, bool pressed, char32_t text, bool ctrl,
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
    if (key == Key::SHIFT) shift_held = pressed;
    if (key == Key::CONTROL) ctrl_held = pressed;
    if (key == Key::ALT) alt_held = pressed;
}

void LinuxTerminal::emit_mouse_button(MouseButton button, bool pressed) {
    InputEvent event;
    event.type = InputEvent::Type::MOUSE_BUTTON;
    event.mouse_button = button;
    event.pressed = pressed;
    event.shift = shift_held;
    event.ctrl = ctrl_held;
    event.alt = alt_held;
    event.mouse_pos = mouse_pos;
    input_events.push_back(event);
}

void LinuxTerminal::emit_mouse_motion() {
    InputEvent event;
    event.type = InputEvent::Type::MOUSE_MOTION;
    event.pressed = false;
    event.shift = shift_held;
    event.ctrl = ctrl_held;
    event.alt = alt_held;
    event.mouse_pos = mouse_pos;
    input_events.push_back(event);
}

std::size_t LinuxTerminal::consume_input_bytes() {
    std::size_t index = 0;

    while (index < pending_bytes.size()) {
        const unsigned char byte = static_cast<unsigned char>(pending_bytes[index]);

        // --- A sequence, or the start of one ---------------------------------
        if (byte == '\x1b') {
            const std::string rest = pending_bytes.substr(index);

            if (rest.size() == 1) {
                // A lone ESC at the end of the read is ambiguous; leave it for
                // poll_events.
                return index;
            }

            // SGR mouse report, handled before the key tables: it also starts with
            // ESC [ and would look like a partial CSI.
            if (rest.size() >= 3 && rest[1] == '[' && rest[2] == '<') {
                std::size_t end = 3;
                while (end < rest.size() && rest[end] != 'M' && rest[end] != 'm') ++end;
                if (end >= rest.size()) return index;  // incomplete, wait

                parse_sgr_mouse(rest.c_str(), static_cast<int>(end + 1));
                index += end + 1;
                continue;
            }

            Key key = Key::UNKNOWN;
            bool shift = false, alt = false, ctrl = false;
            const std::size_t used = keyseq::match_escape(rest, key, shift, alt, ctrl);
            if (used == 0) {
                // A complete CSI/SS3 sequence we do not map: drop it whole.
                const std::size_t unknown = keyseq::unknown_sequence_length(rest);
                if (unknown > 0) {
                    index += unknown;
                    continue;
                }
                // An incomplete sequence of a recognised shape: wait for the rest.
                const bool sequence_start =
                    rest.size() >= 2 && (rest[1] == '[' || rest[1] == 'O');
                if (sequence_start) return index;

                // Otherwise the ESC was the Escape key; the next byte is its own
                // key (Alt+letter on the wire).
                emit_key(Key::ESCAPE, true, 0, false, false, false);
                ++index;
                continue;
            }

            // A terminal cannot report a key release, so a named key is a press.
            emit_key(key, true, 0, ctrl, shift, alt);
            index += used;
            continue;
        }

        // --- Control codes ----------------------------------------------------
        // What a terminal sends for ctrl+letter and the editing keys.
        Key control_key = Key::NONE;
        bool is_ctrl_letter = false;
        switch (byte) {
            case 0x09: control_key = Key::TAB; break;
            case 0x0D: control_key = Key::ENTER; break;
            case 0x0A: control_key = Key::ENTER; break;
            case 0x7F: control_key = Key::BACKSPACE; break;
            case 0x08: control_key = Key::BACKSPACE; break;
            default:
                // 0x01..0x1A are Ctrl+A..Ctrl+Z; 0x00 is Ctrl+Space.
                if (byte >= 0x01 && byte <= 0x1A) {
                    control_key = static_cast<Key>(static_cast<uint16_t>(Key::A) + (byte - 1));
                    is_ctrl_letter = true;
                } else if (byte == 0x00) {
                    control_key = Key::SPACE;
                    is_ctrl_letter = true;
                }
                break;
        }
        if (control_key != Key::NONE) {
            emit_key(control_key, true, 0, is_ctrl_letter, false, false);
            ++index;
            continue;
        }

        // --- Printable --------------------------------------------------------
        char32_t codepoint = 0;
        const std::size_t length = keyseq::decode_utf8(pending_bytes, index, codepoint);
        if (length == 0) return index;  // incomplete multibyte, wait for the rest

        const Key key = keyseq::key_from_ascii(byte);
        emit_key(key == Key::NONE ? Key::UNKNOWN : key, true, codepoint, false,
                 false, false);
        index += length;
    }

    return index;
}

void LinuxTerminal::init() {
    // Store original terminal settings
    tcgetattr(STDIN_FILENO, &original_termios);
    termios_stored = true;

    // 1. Switch to alternate screen buffer FIRST (so everything appears there)
    write(STDOUT_FILENO, "\x1b[?1049h", 8);
    // 2. Hide cursor
    write(STDOUT_FILENO, "\x1b[?25l", 6);
    // 3. Now enable raw mode and mouse tracking
    enable_raw_mode();
    enable_mouse_tracking();
}

void LinuxTerminal::restore() {
    // Show cursor
    write(STDOUT_FILENO, "\x1b[?25h", 6);
    // Switch back from alternate screen
    write(STDOUT_FILENO, "\x1b[?1049l", 8);

    disable_mouse_tracking();
    disable_raw_mode();
}

void LinuxTerminal::set_cursor_visible(bool visible) {
    if (visible) {
        write(STDOUT_FILENO, "\x1b[?25h", 6);
    } else {
        write(STDOUT_FILENO, "\x1b[?25l", 6);
    }
}

void LinuxTerminal::set_cursor_position(Vec2 pos) {
    // ANSI escape positions are 1-based
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "\x1b[%d;%d_h", pos.y + 1, pos.x + 1);
    write(STDOUT_FILENO, buf, len);
}

Vec2 LinuxTerminal::get_screen_size(Vec2 fallback) const {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        return {static_cast<int>(ws.ws_col), static_cast<int>(ws.ws_row)};
    }
    return fallback;
}

void LinuxTerminal::present_buffer(const std::vector<TermChar>& buffer, Vec2 buffer_size) {
    // Query actual terminal dimensions so we never write past them
    struct winsize ws;
    int term_w = buffer_size.x;
    int term_h = buffer_size.y;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        term_w = std::min(buffer_size.x, (int)ws.ws_col);
        term_h = std::min(buffer_size.y, (int)ws.ws_row);
    }

    std::string frame;
    frame.reserve(term_w * term_h * 20);

    // Move cursor to top-left (do not clear entire screen to avoid blinking)
    frame += "\x1b[H";

    uint16_t prev_fg = 255, prev_bg = 255;

    for (int y = 0; y < term_h; ++y) {
        // Move cursor to start of this row explicitly (avoids any wrap issues)
        char row_buf[16];
        int rlen = snprintf(row_buf, sizeof(row_buf), "\x1b[%d;1H", y + 1);
        frame.append(row_buf, rlen);

        for (int x = 0; x < term_w; ++x) {
            int idx = y * buffer_size.x + x;
            if (idx >= (int)buffer.size()) continue;
            const TermChar& tc = buffer[idx];

            if (tc.foreground != prev_fg || tc.background != prev_bg) {
                char color_buf[32];
                int clen = snprintf(color_buf, sizeof(color_buf), "\x1b[%d;%dm",
                    color_id_to_ansi_fg(tc.foreground),
                    color_id_to_ansi_bg(tc.background));
                frame.append(color_buf, clen);
                prev_fg = tc.foreground;
                prev_bg = tc.background;
            }

            // Encode wchar_t as UTF-8
            wchar_t wc = tc.character;
            if (wc < 128) {
                frame += static_cast<char>(wc);
            } else if (wc < 0x800) {
                frame += static_cast<char>(0xC0 | (wc >> 6));
                frame += static_cast<char>(0x80 | (wc & 0x3F));
            } else {
                frame += static_cast<char>(0xE0 | (wc >> 12));
                frame += static_cast<char>(0x80 | ((wc >> 6) & 0x3F));
                frame += static_cast<char>(0x80 | (wc & 0x3F));
            }
        }
        frame += "\x1b[0m\x1b[K"; // Reset colors and clear to end of line
        prev_fg = 255; prev_bg = 255; // Force color update on next line
    }

    frame += "\x1b[0m\x1b[J"; // Clear to end of screen
    write(STDOUT_FILENO, frame.c_str(), frame.size());
}

void LinuxTerminal::poll_events() {
    l_click_prev = l_click;
    r_click_prev = r_click;
    // Per-poll, so a key press or wheel notch belongs to exactly one frame.
    keys_pressed.clear();
    wheel_delta = 0;

    // Read available bytes from stdin (non-blocking)
    char buf[256];
    const int n = read(STDIN_FILENO, buf, sizeof(buf));
    if (n > 0) pending_bytes.append(buf, static_cast<std::size_t>(n));

    // A lone ESC that outlived one poll was the Escape key all along; the
    // sequence it might have started never arrived.
    if (pending_bytes == "\x1b") {
        if (esc_waiting) {
            emit_key(Key::ESCAPE, true, 0, false, false, false);
            pending_bytes.clear();
            esc_waiting = false;
        } else {
            esc_waiting = true;
        }
    } else {
        esc_waiting = false;
        const std::size_t used = consume_input_bytes();
        pending_bytes.erase(0, used);
    }

    l_click_just_pressed = (l_click && !l_click_prev);
    r_click_just_pressed = (r_click && !r_click_prev);
}

std::vector<InputEvent> LinuxTerminal::take_input_events() {
    // Swapped rather than copied: the terminal keeps an empty vector, so an
    // event reaches exactly one drain.
    std::vector<InputEvent> drained;
    drained.swap(input_events);
    return drained;
}

bool LinuxTerminal::was_key_pressed(Key key) const {
    for (Key pressed : keys_pressed) {
        if (pressed == key) return true;
    }
    return false;
}

int LinuxTerminal::get_wheel_delta() const { return wheel_delta; }

Vec2 LinuxTerminal::get_mouse_position() const { return mouse_pos; }
bool LinuxTerminal::is_left_click_pressed() const { return l_click; }
bool LinuxTerminal::is_right_click_pressed() const { return r_click; }
bool LinuxTerminal::is_left_click_just_pressed() const { return l_click_just_pressed; }
bool LinuxTerminal::is_right_click_just_pressed() const { return r_click_just_pressed; }

std::string LinuxTerminal::get_line_input(Vec2 position) {
    // Temporarily restore canonical mode for line input
    disable_mouse_tracking();

    struct termios line_mode;
    tcgetattr(STDIN_FILENO, &line_mode);
    line_mode.c_lflag |= (ECHO | ICANON);
    line_mode.c_cc[VMIN] = 1;
    line_mode.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &line_mode);

    set_cursor_position(position);
    set_cursor_visible(true);

    std::string input;
    std::getline(std::cin, input);

    set_cursor_visible(false);

    // Re-enable raw mode and mouse tracking
    enable_raw_mode();
    enable_mouse_tracking();

    // Whatever was buffered as raw bytes while the line was typed is not input
    // the caller wants: it is the same line echoed a second time.
    pending_bytes.clear();

    return input;
}

} // namespace guayaba

#endif // __linux__ || __APPLE__