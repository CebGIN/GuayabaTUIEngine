#include <algorithm>
#include <string>
#include <vector>

#include <guayaba/Core/Utf8.hpp>
#include <guayaba/Renderer/ConsoleRenderer.hpp>

#include <guayaba/Terminal/IOSTerminal.hpp>

namespace guayaba {

namespace {

// The thickest frame that still leaves a cell of content on each side.
int max_border_width(const Rect& area) {
    return std::min(area.size.x, area.size.y) <= 1
               ? 0
               : (std::min(area.size.x, area.size.y) + 1) / 2;
}

struct BorderGlyphs {
    wchar_t horizontal;
    wchar_t vertical;
    wchar_t top_left;
    wchar_t top_right;
    wchar_t bottom_left;
    wchar_t bottom_right;
};

BorderGlyphs glyphs_for(StyleBorder style, int corner_radius) {
    switch (style) {
        case StyleBorder::ASCII:
            return {L'-', L'|', L'+', L'+', L'+', L'+'};
        case StyleBorder::DOUBLE:
            return {L'═', L'║', L'╔', L'╗', L'╚', L'╝'};
        case StyleBorder::HEAVY:
            return {L'█', L'█', L'█', L'█', L'█', L'█'};
        // Rounded corners only exist once a radius is set.
        case StyleBorder::ROUNDED:
            if (corner_radius > 0) return {L'─', L'│', L'╭', L'╮', L'╰', L'╯'};
            return {L'-', L'|', L'+', L'+', L'+', L'+'};
        case StyleBorder::DASHED:
        case StyleBorder::NONE:
        default:
            return {L'-', L'|', L'+', L'+', L'+', L'+'};
    }
}

// Picks the joint glyph for a corner cell. At cell resolution a corner is one
// cell, so corner_radius only chooses between arc joints and square ones.
wchar_t corner_glyph(const BorderGlyphs& g, bool on_top, bool on_left) {
    if (on_top) return on_left ? g.top_left : g.top_right;
    return on_left ? g.bottom_left : g.bottom_right;
}

} // namespace

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
    empty_char.background = 15;  // BRIGHT_WHITE

    for (size_t i = 0; i < screen_buffer.size(); ++i) {
        screen_buffer[i] = empty_char;
    }
}

void ConsoleRenderer::put_char(Vec2 pos, wchar_t character, uint16_t fg, uint16_t bg,
                               const Rect& clip) {
    // The buffer bound and the clip are independent.
    if (clip.is_empty()) return;
    if (pos.x < 0 || pos.x >= console_buffer_size.x ||
        pos.y < 0 || pos.y >= console_buffer_size.y) return;
    if (!clip.contains(pos)) return;

    int index = pos.y * console_buffer_size.x + pos.x;
    screen_buffer[index].character = character;
    screen_buffer[index].foreground = fg;
    screen_buffer[index].background = bg;
}

void ConsoleRenderer::put_text(Vec2 origin, const std::vector<std::string>& lines, uint16_t fg,
                               uint16_t bg, const Rect& clip) {
    if (lines.empty() || clip.is_empty()) return;

    // Widths are codepoint counts, not byte counts: a multi-byte character is one cell.
    int widest = 0;
    for (const std::string& line : lines) {
        widest = std::max(widest, text_width(line));
    }
    if (widest <= 0) return;

    const Rect content{origin, Vec2{widest, (int)lines.size()}};
    const Rect visible = content.intersection(clip);
    if (visible.is_empty()) return;

    // Offsets and limits, so the loop walks only what is on screen.
    const int ox = visible.left() - content.left();
    const int oy = visible.top() - content.top();
    const int last_col = ox + visible.size.x;

    for (int row = 0; row < visible.size.y; ++row) {
        const int line_index = oy + row;
        if (line_index >= (int)lines.size()) break;

        const std::string& line = lines[line_index];
        // Walk codepoints so one glyph is one column. Columns left of the window
        // are decoded but not drawn.
        std::size_t byte = 0;
        int col = 0;
        while (byte < line.size() && col < last_col) {
            char32_t codepoint = 0;
            const std::size_t length = decode_utf8(line, byte, codepoint);
            if (col >= ox) {
                put_char(Vec2{content.left() + col, content.top() + line_index},
                         static_cast<wchar_t>(codepoint), fg, bg, visible);
            }
            byte += length;
            ++col;
        }
    }
}

void ConsoleRenderer::fill_rect(const Rect& area, wchar_t ch, uint16_t fg, uint16_t bg,
                               const Rect& clip) {
    const Rect visible = area.intersection(clip);
    if (visible.is_empty()) return;

    for (int y = visible.top(); y < visible.bottom(); ++y) {
        for (int x = visible.left(); x < visible.right(); ++x) {
            put_char(Vec2{x, y}, ch, fg, bg, visible);
        }
    }
}

void ConsoleRenderer::draw_box(const Rect& area, StyleBorder style, uint16_t fg, uint16_t bg,
                               int width, int corner_radius, const Rect& clip) {
    if (area.is_empty() || clip.is_empty()) return;

    const BorderGlyphs g = glyphs_for(style, corner_radius);
    const int rings = std::clamp(width, 0, max_border_width(area));

    // Each cell of thickness is a ring inset by one more cell than the last.
    for (int ring = 0; ring < rings; ++ring) {
        const Rect band = area.grown(-ring, -ring);
        if (band.is_empty()) break;

        // Walk only the part of the ring that survives the clip; corners keep
        // their own band geometry.
        const Rect visible = band.intersection(clip);
        if (visible.is_empty()) continue;

        const int x0 = band.left();
        const int x1 = band.right() - 1;
        const int y0 = band.top();
        const int y1 = band.bottom() - 1;

        for (int y = visible.top(); y < visible.bottom(); ++y) {
            for (int x = visible.left(); x < visible.right(); ++x) {
                const bool on_top = (y == y0);
                const bool on_bottom = (y == y1);
                const bool on_left = (x == x0);
                const bool on_right = (x == x1);

                // Never repaint the interior: the fill owns those cells.
                if (!on_top && !on_bottom && !on_left && !on_right) continue;

                wchar_t glyph = 0;
                if (on_top || on_bottom) glyph = g.horizontal;
                if (on_left || on_right) glyph = g.vertical;

                if ((on_top || on_bottom) && (on_left || on_right)) {
                    glyph = corner_glyph(g, on_top, on_left);
                } else if (style == StyleBorder::DASHED) {
                    // Phase each run from the cell it starts at, so every edge
                    // opens with a dash.
                    const int offset = (on_top || on_bottom) ? (x - x0) : (y - y0);
                    if (offset % 2 != 0) continue;
                }

                if (glyph != 0) put_char(Vec2{x, y}, glyph, fg, bg, visible);
            }
        }
    }
}

void ConsoleRenderer::present() {
    if (terminal_instance) {
        terminal_instance->present_buffer(screen_buffer, console_buffer_size);
    }
}

} // namespace guayaba