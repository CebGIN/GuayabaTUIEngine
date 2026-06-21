#ifndef GUAYABA_TERMINAL_WINDOWS_HPP
#define GUAYABA_TERMINAL_WINDOWS_HPP

#ifdef _WIN32

#include <windows.h>

class WindowsTerminal : public IOSTerminal {
private:
    HANDLE hStdin = INVALID_HANDLE_VALUE;
    HANDLE hStdout = INVALID_HANDLE_VALUE;
    DWORD prevMode = 0;

    Vec2 mousePos = {0, 0};
    bool lClick = false;
    bool rClick = false;
    bool lClickPrev = false;
    bool rClickPrev = false;
    bool lClickJustPressed = false;
    bool rClickJustPressed = false;

    // Convert our platform-neutral color ID (0-15) to Windows WORD foreground attribute
    static WORD colorIdToWinFg(uint16_t colorId) {
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
        return table[colorId & 0x0F];
    }

    static WORD colorIdToWinBg(uint16_t colorId) {
        return colorIdToWinFg(colorId) << 4;
    }

public:
    void init() override {
        hStdin = GetStdHandle(STD_INPUT_HANDLE);
        hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hStdin == INVALID_HANDLE_VALUE || hStdout == INVALID_HANDLE_VALUE) return;

        if (!GetConsoleMode(hStdin, &prevMode)) return;
        DWORD newMode = prevMode;
        newMode &= ~ENABLE_QUICK_EDIT_MODE;
        newMode |= ENABLE_EXTENDED_FLAGS | ENABLE_MOUSE_INPUT;
        SetConsoleMode(hStdin, newMode);

        CONSOLE_CURSOR_INFO cci;
        cci.dwSize = 1;
        cci.bVisible = FALSE;
        SetConsoleCursorInfo(hStdout, &cci);
    }

    void restore() override {
        if (hStdin != INVALID_HANDLE_VALUE) {
            SetConsoleMode(hStdin, prevMode);
        }
        setCursorVisible(true);
    }

    void setCursorVisible(bool visible) override {
        if (hStdout == INVALID_HANDLE_VALUE) return;
        CONSOLE_CURSOR_INFO cci;
        cci.dwSize = 1;
        cci.bVisible = visible ? TRUE : FALSE;
        SetConsoleCursorInfo(hStdout, &cci);
    }

    void setCursorPosition(Vec2 pos) override {
        if (hStdout == INVALID_HANDLE_VALUE) return;
        COORD c = {static_cast<SHORT>(pos.x), static_cast<SHORT>(pos.y)};
        SetConsoleCursorPosition(hStdout, c);
    }

    void presentBuffer(const std::vector<TermChar>& buffer, Vec2 bufferSize) override {
        if (hStdout == INVALID_HANDLE_VALUE) return;

        // Convert TermChar buffer to CHAR_INFO buffer
        std::vector<CHAR_INFO> winBuffer(buffer.size());
        for (size_t i = 0; i < buffer.size(); ++i) {
            winBuffer[i].Char.UnicodeChar = buffer[i].character;
            winBuffer[i].Attributes = colorIdToWinFg(buffer[i].foreground) | colorIdToWinBg(buffer[i].background);
        }

        COORD size = {static_cast<SHORT>(bufferSize.x), static_cast<SHORT>(bufferSize.y)};
        SMALL_RECT writeRegion = {0, 0, static_cast<SHORT>(bufferSize.x - 1), static_cast<SHORT>(bufferSize.y - 1)};

        if (!WriteConsoleOutputW(hStdout, winBuffer.data(), size, {0, 0}, &writeRegion)) {
            std::cerr << "Error writing to console output: " << GetLastError() << std::endl;
        }
    }

    void pollEvents() override {
        lClickPrev = lClick;
        rClickPrev = rClick;

        lClick = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        rClick = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

        lClickJustPressed = (lClick && !lClickPrev);
        rClickJustPressed = (rClick && !rClickPrev);

        const int EVENT_COUNT = 128;
        INPUT_RECORD inputBuffer[EVENT_COUNT];
        DWORD eventsRead = 0;
        DWORD eventsAvailable = 0;

        GetNumberOfConsoleInputEvents(hStdin, &eventsAvailable);
        if (eventsAvailable > 0) {
            if (!ReadConsoleInput(hStdin, inputBuffer, EVENT_COUNT, &eventsRead)) return;

            for (DWORD i = 0; i < eventsRead; ++i) {
                INPUT_RECORD &rec = inputBuffer[i];
                if (rec.EventType == MOUSE_EVENT) {
                    MOUSE_EVENT_RECORD &mouseRec = rec.Event.MouseEvent;
                    if (mouseRec.dwEventFlags == MOUSE_MOVED) {
                        mousePos = {mouseRec.dwMousePosition.X, mouseRec.dwMousePosition.Y};
                    }
                }
            }
            if (eventsAvailable > eventsRead) {
                FlushConsoleInputBuffer(hStdin);
            }
        }
    }

    Vec2 getMousePosition() const override { return mousePos; }
    bool isLeftClickPressed() const override { return lClick; }
    bool isRightClickPressed() const override { return rClick; }
    bool isLeftClickJustPressed() const override { return lClickJustPressed; }
    bool isRightClickJustPressed() const override { return rClickJustPressed; }

    std::string getLineInput(Vec2 position) override {
        std::string input;
        setCursorPosition(position);
        setCursorVisible(true);
        std::getline(std::cin, input);
        setCursorVisible(false);
        return input;
    }
};

inline std::unique_ptr<IOSTerminal> CreateSystemTerminal() {
    return std::make_unique<WindowsTerminal>();
}

#endif // _WIN32

#endif // GUAYABA_TERMINAL_WINDOWS_HPP
