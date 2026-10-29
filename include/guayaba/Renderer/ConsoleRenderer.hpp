#ifndef GUAYABA_RENDERER_CONSOLERENDERER_HPP
#define GUAYABA_RENDERER_CONSOLERENDERER_HPP

#include <cstdint>
#include <string>
#include <vector>

#include <guayaba/Core/Rect.hpp>
#include <guayaba/Core/TermChar.hpp>
#include <guayaba/Core/Vec2.hpp>

namespace guayaba {

class IOSTerminal;

// How a frame draws its edges. The renderer knows how to draw each appearance;
// StyleBox only decides whether and with what theme to ask for one.
enum class StyleBorder {
    NONE,      // no border, whatever the width says
    ASCII,     // | -
    DOUBLE,    // ║ ═
    ROUNDED,   // ╭ ─ ╮, when a corner radius is set
    HEAVY,     // █ ▓
    DASHED,    // broken-up horizontal runs
};

class ConsoleRenderer {
private:
    Vec2 console_buffer_size;
    std::vector<TermChar> screen_buffer;
    IOSTerminal* terminal_instance;

public:
    ConsoleRenderer(Vec2 buffer_size, IOSTerminal* terminal);

    Vec2 get_buffer_size() const;

    // Reallocates the back buffer, e.g. after a resize. No-op when the size is unchanged.
    void resize(Vec2 new_size);

    void clear_buffer();

    // Clipping is passed per primitive, never held as renderer state, so a draw
    // call is a pure function of its arguments. Each primitive intersects its
    // content against the clip and iterates only the visible window. An empty clip
    // draws nothing. These batch primitives are what nodes normally use.
    void put_text(Vec2 origin, const std::vector<std::string>& lines, uint16_t fg,
                  uint16_t bg, const Rect& clip);

    // Paints every cell of area with ch.
    void fill_rect(const Rect& area, wchar_t ch, uint16_t fg, uint16_t bg,
                   const Rect& clip);

    // Draws a frame around area, `width` cells thick. corner_radius picks arc
    // joints for StyleBorder::ROUNDED and is ignored by other styles.
    void draw_box(const Rect& area, StyleBorder style, uint16_t fg, uint16_t bg, int width,
                  int corner_radius, const Rect& clip);

    // Per-cell drawing, for glyphs a batch primitive cannot compute. Clips this cell.
    void put_char(Vec2 pos, wchar_t character, uint16_t fg, uint16_t bg, const Rect& clip);

    void present();
};

} // namespace guayaba

#endif // GUAYABA_RENDERER_CONSOLERENDERER_HPP