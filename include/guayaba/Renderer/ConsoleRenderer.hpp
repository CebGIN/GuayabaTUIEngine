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

// How a frame draws its edges. This lives with the renderer because box drawing
// is a terminal idiom (ncurses has box(), SDL has DrawRect): the renderer knows
// how to render each appearance, while StyleBox only decides whether and with
// what theme to ask for one.
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

    // Reallocates the back buffer, e.g. after a terminal resize. No-op if the
    // size is unchanged, so it is safe to call every frame.
    void resize(Vec2 new_size);

    void clear_buffer();

    // Clipping is passed per primitive, never held as renderer state. The reason
    // is that it keeps a draw call self-contained: what it writes is a pure
    // function of its arguments, with no hidden mode inherited from whatever
    // was drawn before it. So no sibling can leave a region dirty for the next
    // one, there is no push/pop discipline to get wrong, and the renderer can be
    // driven from anywhere without setup. Batch primitives are what make this
    // affordable - a node issues one or two calls per region, not one per cell.
    // An empty clip draws nothing.
    //
    // Each primitive intersects its content against the clip up front and then
    // iterates only that visible window, so a partly offscreen box or a partly
    // scrolled label costs no more than what actually lands on screen.
    //
    // Batch primitives - hand over the whole content and let the renderer
    // slice it. These are what nodes normally use.
    void put_text(Vec2 origin, const std::vector<std::string>& lines, uint16_t fg,
                  uint16_t bg, const Rect& clip);

    // Paints every cell of area with ch. Intersects against clip first, so a
    // fill larger than the screen only touches what fits.
    void fill_rect(const Rect& area, wchar_t ch, uint16_t fg, uint16_t bg,
                   const Rect& clip);

    // Draws a frame around area, `width` cells thick, in the requested style.
    // Corners, dash phasing and multi-ring thickness are handled here; the
    // caller passes only the appearance and colors. corner_radius picks arc
    // joints for StyleBorder::ROUNDED and is ignored by every other style.
    void draw_box(const Rect& area, StyleBorder style, uint16_t fg, uint16_t bg, int width,
                  int corner_radius, const Rect& clip);

    // Fine-grained escape hatch for procedural or per-cell drawing, where the
    // glyph at a cell depends on something a batch primitive cannot see (the
    // dashed border's corner joints are the in-tree case). Clips this one cell.
    void put_char(Vec2 pos, wchar_t character, uint16_t fg, uint16_t bg, const Rect& clip);

    void present();
};

} // namespace guayaba

#endif // GUAYABA_RENDERER_CONSOLERENDERER_HPP