// Headless checks for the simple controls added in Fase 1: ProgressBar, Spinner,
// CheckBox and Timer. Drawing is asserted against a capturing terminal; nothing
// opens a real one.

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include <guayaba/Guayaba.hpp>

using namespace guayaba;

namespace {

constexpr Vec2 SCREEN{20, 5};
constexpr Vec2 WIDE{40, 5};

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

// --- ProgressBar ------------------------------------------------------------

void draw_progress(ProgressBar& bar, ConsoleRenderer& renderer) {
    renderer.clear_buffer();
    bar.draw(renderer, Rect::from_size(renderer.get_buffer_size()));
    renderer.present();
}

int first_filled(const CapturingTerminal& terminal, int width) {
    for (int x = 0; x < width; ++x) {
        if (terminal.glyph(x, 0) == L'█') return x;
    }
    return -1;
}

int count_filled(const CapturingTerminal& terminal, int width) {
    int count = 0;
    for (int x = 0; x < width; ++x) {
        if (terminal.glyph(x, 0) == L'█') ++count;
    }
    return count;
}

void test_progress_value_is_clamped() {
    ProgressBar bar;
    bar.set_range(0, 10);
    assert(bar.get_value() == 0);

    bar.set_value(-5);
    assert(bar.get_value() == 0);

    bar.set_value(50);
    assert(bar.get_value() == 10);

    bar.set_value(4);
    assert(bar.get_value() == 4);
}

void test_progress_degenerate_range_draws_empty() {
    ProgressBar bar;
    bar.set_size({10, 1});
    bar.set_range(5, 5);
    bar.set_value(100);
    assert(bar.get_value() == 5);

    CapturingTerminal terminal;
    ConsoleRenderer renderer(SCREEN, &terminal);
    bar.update_layout(Rect::from_size(SCREEN));
    draw_progress(bar, renderer);

    assert(first_filled(terminal, 10) == -1);
}

void test_progress_fills_floor_cells() {
    ProgressBar bar;
    bar.set_size({10, 1});
    bar.set_range(0, 100);
    bar.set_value(55);  // 5.5 cells, floored to 5

    CapturingTerminal terminal;
    ConsoleRenderer renderer(SCREEN, &terminal);
    bar.update_layout(Rect::from_size(SCREEN));
    draw_progress(bar, renderer);

    assert(count_filled(terminal, 10) == 5);
    assert(terminal.glyph(4, 0) == L'█');
    assert(terminal.glyph(5, 0) == L' ');
}

void test_progress_signal_only_on_change() {
    ProgressBar bar;
    bar.set_range(0, 10);

    int changes = 0;
    double last = -1.0;
    bar.connect_signal<double>("value_changed", [&](double new_value) {
        ++changes;
        last = new_value;
    });

    bar.set_value(5);
    bar.set_value(5);
    bar.set_value(11);  // clamped to 10

    assert(changes == 2);
    assert(last == 10.0);
}

void test_progress_indeterminate_moves_with_delta() {
    ProgressBar bar;
    bar.set_size({40, 1});
    bar.set_indeterminate(true);

    CapturingTerminal terminal;
    ConsoleRenderer renderer(WIDE, &terminal);
    bar.update_layout(Rect::from_size(WIDE));

    // At phase zero the block is still just off the left edge.
    draw_progress(bar, renderer);
    assert(first_filled(terminal, 40) == -1);

    // One second at 12 cells/s puts the block at column 2, ten cells wide.
    bar.update(1.0);
    draw_progress(bar, renderer);
    assert(first_filled(terminal, 40) == 2);
    assert(count_filled(terminal, 40) == 10);

    // Half a second more: column 8.
    bar.update(0.5);
    draw_progress(bar, renderer);
    assert(first_filled(terminal, 40) == 8);
    assert(count_filled(terminal, 40) == 10);
}

void test_progress_hidden_does_not_advance() {
    ProgressBar bar;
    bar.set_size({40, 1});
    bar.set_indeterminate(true);

    CapturingTerminal terminal;
    ConsoleRenderer renderer(WIDE, &terminal);
    bar.update_layout(Rect::from_size(WIDE));

    bar.update(1.0);
    draw_progress(bar, renderer);
    assert(first_filled(terminal, 40) == 2);

    // Hidden: the wrapper stops on_update, so the phase does not move.
    bar.set_visible(false);
    bar.update(5.0);
    bar.set_visible(true);
    bar.update_layout(Rect::from_size(WIDE));

    draw_progress(bar, renderer);
    assert(first_filled(terminal, 40) == 2);
}

void test_progress_percentage_and_custom_text() {
    ProgressBar bar;
    bar.set_size({10, 1});
    bar.set_range(0, 100);
    bar.set_value(50);
    bar.set_show_percentage(true);

    CapturingTerminal terminal;
    ConsoleRenderer renderer(SCREEN, &terminal);
    bar.update_layout(Rect::from_size(SCREEN));
    draw_progress(bar, renderer);

    // "50%" is three cells, centred: (10 - 3) / 2 = 3.
    assert(terminal.glyph(3, 0) == L'5');
    assert(terminal.glyph(4, 0) == L'0');
    assert(terminal.glyph(5, 0) == L'%');

    // A custom text wins over the percentage.
    bar.set_text("ok");
    draw_progress(bar, renderer);
    assert(terminal.glyph(4, 0) == L'o');
    assert(terminal.glyph(5, 0) == L'k');
}

} // namespace

int main() {
    test_progress_value_is_clamped();
    test_progress_degenerate_range_draws_empty();
    test_progress_fills_floor_cells();
    test_progress_signal_only_on_change();
    test_progress_indeterminate_moves_with_delta();
    test_progress_hidden_does_not_advance();
    test_progress_percentage_and_custom_text();

    std::cout << "basic_controls: all checks passed." << std::endl;
    return 0;
}
