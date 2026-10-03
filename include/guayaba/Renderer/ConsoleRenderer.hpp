#ifndef GUAYABA_RENDERER_CONSOLERENDERER_HPP
#define GUAYABA_RENDERER_CONSOLERENDERER_HPP

#include <cstdint>
#include <string>
#include <vector>

#include <guayaba/Core/TermChar.hpp>
#include <guayaba/Core/Vec2.hpp>

namespace guayaba {

class IOSTerminal;

class ConsoleRenderer {
private:
    Vec2 console_buffer_size;
    std::vector<TermChar> screen_buffer;
    IOSTerminal* terminal_instance;

public:
    ConsoleRenderer(Vec2 buffer_size, IOSTerminal* terminal);

    Vec2 get_buffer_size() const;

    // Reallocates the back buffer, e.g. after a terminal resize. No-op if the
    // size is unchanged, so it is safe to call every frame.
    void resize(Vec2 new_size);

    void clear_buffer();

    void put_char(Vec2 pos, wchar_t character, uint16_t fg, uint16_t bg);

    void put_string(Vec2 pos, const std::string& text, uint16_t fg, uint16_t bg);

    void present();
};

} // namespace guayaba

#endif // GUAYABA_RENDERER_CONSOLERENDERER_HPP