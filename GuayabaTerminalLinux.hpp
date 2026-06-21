#ifndef GUAYABA_TERMINAL_LINUX_HPP
#define GUAYABA_TERMINAL_LINUX_HPP

#if defined(__linux__) || defined(__APPLE__)

#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstdio>
#include <cstring>
#include <sstream>

class LinuxTerminal : public IOSTerminal {
private:
    struct termios originalTermios;
    bool termiosStored = false;

    Vec2 mousePos = {0, 0};
    bool lClick = false;
    bool rClick = false;
    bool lClickPrev = false;
    bool rClickPrev = false;
    bool lClickJustPressed = false;
    bool rClickJustPressed = false;

    // Map color ID (0-15) to ANSI foreground code (30-37, 90-97)
    static int colorIdToAnsiFg(uint16_t id) {
        // 0-7 -> 30-37,  8-15 -> 90-97
        if (id < 8) return 30 + id;
        return 90 + (id - 8);
    }

    // Map color ID (0-15) to ANSI background code (40-47, 100-107)
    static int colorIdToAnsiBg(uint16_t id) {
        if (id < 8) return 40 + id;
        return 100 + (id - 8);
    }

    // Map ANSI color order to match our Color IDs:
    // Our:  0=Black, 1=Red, 2=Green, 3=Yellow, 4=Blue, 5=Magenta, 6=Cyan, 7=White
    // ANSI: 0=Black, 1=Red, 2=Green, 3=Yellow, 4=Blue, 5=Magenta, 6=Cyan, 7=White
    // They match perfectly!

    void enableRawMode() {
        struct termios raw;
        tcgetattr(STDIN_FILENO, &raw);
        raw.c_lflag &= ~(ECHO | ICANON | ISIG);
        raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
        raw.c_cc[VMIN] = 0;   // Non-blocking read
        raw.c_cc[VTIME] = 0;  // No timeout
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    }

    void disableRawMode() {
        if (termiosStored) {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &originalTermios);
        }
    }

    // Enable xterm mouse tracking (all movement + button events)
    void enableMouseTracking() {
        // Enable button event tracking + SGR extended mode
        write(STDOUT_FILENO, "\x1b[?1003h", 8);  // All motion tracking
        write(STDOUT_FILENO, "\x1b[?1006h", 8);  // SGR extended mouse mode
    }

    void disableMouseTracking() {
        write(STDOUT_FILENO, "\x1b[?1003l", 8);
        write(STDOUT_FILENO, "\x1b[?1006l", 8);
    }

    // Parse SGR mouse event: \x1b[<button;x;y{M|m}
    bool parseSGRMouse(const char* buf, int len) {
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

        bool isRelease = (buf[i] == 'm');
        // bool isPress = (buf[i] == 'M');

        // SGR coordinates are 1-based, convert to 0-based
        mousePos = {cx - 1, cy - 1};

        int baseButton = button & 0x03; // Bottom 2 bits = button
        bool isMotion = (button & 32) != 0;

        if (!isMotion) {
            if (baseButton == 0) { // Left button
                lClick = !isRelease;
            } else if (baseButton == 2) { // Right button
                rClick = !isRelease;
            }
        }

        return true;
    }

public:
    void init() override {
        // Store original terminal settings
        tcgetattr(STDIN_FILENO, &originalTermios);
        termiosStored = true;

        // 1. Switch to alternate screen buffer FIRST (so everything appears there)
        write(STDOUT_FILENO, "\x1b[?1049h", 8);
        // 2. Hide cursor
        write(STDOUT_FILENO, "\x1b[?25l", 6);
        // 3. Now enable raw mode and mouse tracking
        enableRawMode();
        enableMouseTracking();
    }

    void restore() override {
        // Show cursor
        write(STDOUT_FILENO, "\x1b[?25h", 6);
        // Switch back from alternate screen
        write(STDOUT_FILENO, "\x1b[?1049l", 8);

        disableMouseTracking();
        disableRawMode();
    }

    void setCursorVisible(bool visible) override {
        if (visible) {
            write(STDOUT_FILENO, "\x1b[?25h", 6);
        } else {
            write(STDOUT_FILENO, "\x1b[?25l", 6);
        }
    }

    void setCursorPosition(Vec2 pos) override {
        // ANSI escape positions are 1-based
        char buf[32];
        int len = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", pos.y + 1, pos.x + 1);
        write(STDOUT_FILENO, buf, len);
    }

    void presentBuffer(const std::vector<TermChar>& buffer, Vec2 bufferSize) override {
        // Query actual terminal dimensions so we never write past them
        struct winsize ws;
        int termW = bufferSize.x;
        int termH = bufferSize.y;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
            termW = std::min(bufferSize.x, (int)ws.ws_col);
            termH = std::min(bufferSize.y, (int)ws.ws_row);
        }

        std::string frame;
        frame.reserve(termW * termH * 20);

        // Move cursor to top-left (do not clear entire screen to avoid blinking)
        frame += "\x1b[H";

        uint16_t prevFg = 255, prevBg = 255;

        for (int y = 0; y < termH; ++y) {
            // Move cursor to start of this row explicitly (avoids any wrap issues)
            char rowBuf[16];
            int rlen = snprintf(rowBuf, sizeof(rowBuf), "\x1b[%d;1H", y + 1);
            frame.append(rowBuf, rlen);

            for (int x = 0; x < termW; ++x) {
                int idx = y * bufferSize.x + x;
                if (idx >= (int)buffer.size()) continue;
                const TermChar& tc = buffer[idx];

                if (tc.foreground != prevFg || tc.background != prevBg) {
                    char colorBuf[32];
                    int clen = snprintf(colorBuf, sizeof(colorBuf), "\x1b[%d;%dm",
                        colorIdToAnsiFg(tc.foreground),
                        colorIdToAnsiBg(tc.background));
                    frame.append(colorBuf, clen);
                    prevFg = tc.foreground;
                    prevBg = tc.background;
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
            prevFg = 255; prevBg = 255; // Force color update on next line
        }

        frame += "\x1b[0m\x1b[J"; // Clear to end of screen
        write(STDOUT_FILENO, frame.c_str(), frame.size());
    }

    void pollEvents() override {
        lClickPrev = lClick;
        rClickPrev = rClick;

        // Read available bytes from stdin (non-blocking)
        char buf[256];
        int n = read(STDIN_FILENO, buf, sizeof(buf));

        if (n > 0) {
            int i = 0;
            while (i < n) {
                // Look for ESC [ < (SGR mouse sequence)
                if (i + 2 < n && buf[i] == '\x1b' && buf[i+1] == '[' && buf[i+2] == '<') {
                    // Find end of SGR sequence (M or m)
                    int start = i;
                    i += 3;
                    while (i < n && buf[i] != 'M' && buf[i] != 'm') i++;
                    if (i < n) {
                        i++; // Skip M/m
                        parseSGRMouse(buf + start, i - start);
                    }
                } else {
                    i++; // Skip unknown byte
                }
            }
        }

        lClickJustPressed = (lClick && !lClickPrev);
        rClickJustPressed = (rClick && !rClickPrev);
    }

    Vec2 getMousePosition() const override { return mousePos; }
    bool isLeftClickPressed() const override { return lClick; }
    bool isRightClickPressed() const override { return rClick; }
    bool isLeftClickJustPressed() const override { return lClickJustPressed; }
    bool isRightClickJustPressed() const override { return rClickJustPressed; }

    std::string getLineInput(Vec2 position) override {
        // Temporarily restore canonical mode for line input
        disableMouseTracking();
        
        struct termios lineMode;
        tcgetattr(STDIN_FILENO, &lineMode);
        lineMode.c_lflag |= (ECHO | ICANON);
        lineMode.c_cc[VMIN] = 1;
        lineMode.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &lineMode);
        
        setCursorPosition(position);
        setCursorVisible(true);

        std::string input;
        std::getline(std::cin, input);

        setCursorVisible(false);

        // Re-enable raw mode and mouse tracking
        enableRawMode();
        enableMouseTracking();

        return input;
    }
};

inline std::unique_ptr<IOSTerminal> CreateSystemTerminal() {
    return std::make_unique<LinuxTerminal>();
}

#endif // __linux__ || __APPLE__

#endif // GUAYABA_TERMINAL_LINUX_HPP
