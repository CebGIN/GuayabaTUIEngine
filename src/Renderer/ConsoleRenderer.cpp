#include <guayaba/Renderer/ConsoleRenderer.hpp>

#include <guayaba/Terminal/IOSTerminal.hpp>

namespace guayaba {

ConsoleRenderer::ConsoleRenderer(Vec2 buffer_size, IOSTerminal* terminal)
    : console_buffer_size(buffer_size), terminal_instance(terminal)
{
    screen_buffer.resize(buffer_size.x * buffer_size.y);
    clear_buffer();
}

Vec2 ConsoleRenderer::get_buffer_size() const { return console_buffer_size; }

void ConsoleRenderer::resize(Vec2 new_size) {
    if (new_size.x == console_buffer_size.x && new_size.y == console_buffer_size.y) return;
    if (new_size.x < 1 || new_size.y < 1) return;
    console_buffer_size = new_size;
    screen_buffer.resize(new_size.x * new_size.y);
    clear_buffer();
}

void ConsoleRenderer::clear_buffer() {
    TermChar empty_char;
    empty_char.character = L' ';
    empty_char.foreground = 15;  // BRIGHT_WHITE
    empty_char.background = 15;  // BRIGHT_WHITE (white background default, matching original)

    for (size_t i = 0; i < screen_buffer.size(); ++i) {
        screen_buffer[i] = empty_char;
    }
}

void ConsoleRenderer::put_char(Vec2 pos, wchar_t character, uint16_t fg, uint16_t bg) {
    if (pos.x >= 0 && pos.x < console_buffer_size.x &&
        pos.y >= 0 && pos.y < console_buffer_size.y) {
            int index = pos.y * console_buffer_size.x + pos.x;
            screen_buffer[index].character = character;
            screen_buffer[index].foreground = fg;
            screen_buffer[index].background = bg;
        }
}

void ConsoleRenderer::put_string(Vec2 pos, const std::string& text, uint16_t fg, uint16_t bg) {
    for (size_t i = 0; i < text.length(); ++i) {
        put_char(pos + Vec2{static_cast<int>(i), 0}, static_cast<wchar_t>(text[i]), fg, bg);
    }
}

void ConsoleRenderer::present() {
    if (terminal_instance) {
        terminal_instance->present_buffer(screen_buffer, console_buffer_size);
    }
}

} // namespace guayaba