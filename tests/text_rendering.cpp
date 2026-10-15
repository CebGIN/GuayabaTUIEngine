// Headless checks for text measurement and drawing with multi-byte characters.
//
// The bug these guard against: measuring a line by bytes and writing one buffer
// cell per byte. A three-byte glyph (an em dash, an arrow, an ellipsis) then
// reserved three columns and landed in three cells, so the terminal - which draws
// it as one column - left two-cell holes in the line and shifted everything after
// it. Widths and cells are codepoints, not bytes.
//
// No terminal is opened: a fake IOSTerminal captures the buffer present() hands
// it, which is the only way to read glyphs back out of the renderer.

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include <guayaba/Core/TextWrap.hpp>
#include <guayaba/Core/Utf8.hpp>
#include <guayaba/Renderer/ConsoleRenderer.hpp>
#include <guayaba/Terminal/IOSTerminal.hpp>

using namespace guayaba;

namespace {

constexpr Vec2 SCREEN{20, 4};

// Three-byte glyphs, spelled out so the test does not depend on the source
// encoding. Adjacent literals are concatenated so a following hex digit cannot be
// swallowed by the \x escape (which would read as one larger codepoint).
const char* const EM_DASH = "\xE2\x80\x94";
const char* const UP = "\xE2\x86\x91";

// Captures the presented buffer so a test can read the glyph at a cell; every
// other virtual is inert.
class CapturingTerminal : public IOSTerminal {
public:
    std::vector<TermChar> buffer;
    Vec2 size{0, 0};

    void init() override {}
    void restore() override {}
    void set_cursor_visible(bool) override {}
    void set_cursor_position(Vec2) override {}
    void present_buffer(const std::vector<TermChar>& presented, Vec2 presented_size) override {
        buffer = presented;
        size = presented_size;
    }
    Vec2 get_screen_size(Vec2 fallback) const override { return fallback; }
    void poll_events() override {}
    Vec2 get_mouse_position() const override { return {0, 0}; }
    bool is_left_click_pressed() const override { return false; }
    bool is_right_click_pressed() const override { return false; }
    bool is_left_click_just_pressed() const override { return false; }
    bool is_right_click_just_pressed() const override { return false; }
    std::vector<InputEvent> take_input_events() override { return {}; }
    bool was_key_pressed(Key) const override { return false; }
    int get_wheel_delta() const override { return 0; }
    std::string get_line_input(Vec2) override { return {}; }

    wchar_t glyph(int x, int y) const {
        return buffer[static_cast<std::size_t>(y) * size.x + x].character;
    }
};

void test_text_width_counts_codepoints() {
    assert(text_width("plain") == 5);
    assert(text_width("") == 0);
    // "a" + em dash + "b": three cells, five bytes.
    assert(text_width(std::string("a") + EM_DASH + "b") == 3);
    // Two arrows: two cells, six bytes.
    assert(text_width(std::string(UP) + UP) == 2);
    // A malformed pair has no valid lead, so each byte is its own cell rather than
    // desyncing the walk.
    assert(text_width("\xE2\x80") == 2);
}

void test_measure_text_counts_codepoints() {
    // The extent a node reserves is in cells: the dash is one cell, not three.
    assert(measure_text(std::string("a") + EM_DASH + "b").x == 3);
    assert(measure_text("ab").x == 2);
}

void test_put_text_places_one_cell_per_codepoint() {
    CapturingTerminal terminal;
    ConsoleRenderer renderer(SCREEN, &terminal);
    const Rect clip{{0, 0}, SCREEN};

    renderer.put_text({0, 0}, {std::string("a") + EM_DASH + "b"}, 15, 0, clip);
    renderer.present();

    assert(terminal.glyph(0, 0) == L'a');
    assert(terminal.glyph(1, 0) == static_cast<wchar_t>(0x2014));  // the em dash
    assert(terminal.glyph(2, 0) == L'b');
    // The glyph after the dash lands one cell later, not three: no hole.
    assert(terminal.glyph(3, 0) == L' ');
}

void test_wrap_measures_codepoints() {
    // "↑↑ ↑↑" is five cells (two + space + two), so it fits width 5 on one line.
    // Measured by bytes (six + space + six) it would have been split in two.
    const std::vector<std::string> lines = wrap(std::string(UP) + UP + " " + UP + UP, 5);
    assert(lines.size() == 1);

    // One cell over the limit and the second word moves to its own line.
    const std::vector<std::string> wrapped = wrap(std::string(UP) + UP + " " + UP + UP, 4);
    assert(wrapped.size() == 2);
}

}  // namespace

int main() {
    test_text_width_counts_codepoints();
    test_measure_text_counts_codepoints();
    test_put_text_places_one_cell_per_codepoint();
    test_wrap_measures_codepoints();

    std::cout << "text_rendering: all checks passed." << std::endl;
    return 0;
}
