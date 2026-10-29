// Headless checks for the simple controls added in Fase 1: ProgressBar, Spinner,
// CheckBox and Timer. Drawing is asserted against a capturing terminal; nothing
// opens a real one.

#include <cassert>
#include <iostream>
#include <stdexcept>
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

// --- Spinner ----------------------------------------------------------------

void draw_spinner(Spinner& spinner, ConsoleRenderer& renderer) {
    renderer.clear_buffer();
    spinner.draw(renderer, Rect::from_size(renderer.get_buffer_size()));
    renderer.present();
}

void test_spinner_advances_with_delta() {
    Spinner spinner;
    spinner.set_size({1, 1});
    spinner.set_frames({"a", "b", "c"});
    spinner.set_interval(0.1);

    CapturingTerminal terminal;
    ConsoleRenderer renderer(SCREEN, &terminal);
    spinner.update_layout(Rect::from_size(SCREEN));

    draw_spinner(spinner, renderer);
    assert(terminal.glyph(0, 0) == L'a');

    // Below one interval: still the first frame.
    spinner.update(0.05);
    draw_spinner(spinner, renderer);
    assert(terminal.glyph(0, 0) == L'a');

    // Crosses the interval: advances exactly one.
    spinner.update(0.06);
    draw_spinner(spinner, renderer);
    assert(terminal.glyph(0, 0) == L'b');

    // Two intervals at once: advances two, wrapping around.
    spinner.update(0.2);
    draw_spinner(spinner, renderer);
    assert(terminal.glyph(0, 0) == L'a');
}

void test_spinner_stop_and_start() {
    Spinner spinner;
    spinner.set_size({1, 1});
    spinner.set_frames({"a", "b"});
    spinner.set_interval(0.1);

    CapturingTerminal terminal;
    ConsoleRenderer renderer(SCREEN, &terminal);
    spinner.update_layout(Rect::from_size(SCREEN));

    spinner.stop();
    spinner.update(1.0);
    draw_spinner(spinner, renderer);
    assert(terminal.glyph(0, 0) == L'a');

    spinner.start();
    spinner.update(0.1);
    draw_spinner(spinner, renderer);
    assert(terminal.glyph(0, 0) == L'b');
}

void test_spinner_empty_frames_draws_nothing() {
    Spinner spinner;
    spinner.set_size({1, 1});
    spinner.set_frames({});

    CapturingTerminal terminal;
    ConsoleRenderer renderer(SCREEN, &terminal);
    spinner.update_layout(Rect::from_size(SCREEN));

    spinner.update(1.0);
    draw_spinner(spinner, renderer);
    assert(terminal.glyph(0, 0) == L' ');
}

void test_spinner_invalid_interval_falls_back() {
    Spinner spinner;

    spinner.set_interval(0.0);
    assert(spinner.get_interval() == 0.1);

    spinner.set_interval(-2.0);
    assert(spinner.get_interval() == 0.1);

    spinner.set_interval(0.5);
    assert(spinner.get_interval() == 0.5);
}

void test_spinner_hidden_does_not_advance() {
    Spinner spinner;
    spinner.set_size({1, 1});
    spinner.set_frames({"a", "b"});
    spinner.set_interval(0.1);

    CapturingTerminal terminal;
    ConsoleRenderer renderer(SCREEN, &terminal);
    spinner.update_layout(Rect::from_size(SCREEN));

    spinner.set_visible(false);
    spinner.update(1.0);
    spinner.set_visible(true);
    spinner.update_layout(Rect::from_size(SCREEN));

    draw_spinner(spinner, renderer);
    assert(terminal.glyph(0, 0) == L'a');
}

void test_spinner_minimum() {
    Spinner spinner;
    assert((spinner.get_minimum_size() == Vec2{1, 1}));
}

// --- CheckBox ---------------------------------------------------------------

InputEvent left_click(Vec2 pos) {
    InputEvent event;
    event.type = InputEvent::Type::MOUSE_BUTTON;
    event.pressed = true;
    event.mouse_button = MouseButton::LEFT;
    event.mouse_pos = pos;
    return event;
}

InputEvent key_press(Key key) {
    InputEvent event;
    event.type = InputEvent::Type::KEY;
    event.pressed = true;
    event.key = key;
    return event;
}

void draw_checkbox(CheckBox& box, ConsoleRenderer& renderer) {
    renderer.clear_buffer();
    box.draw(renderer, Rect::from_size(renderer.get_buffer_size()));
    renderer.present();
}

void test_checkbox_click_toggles_and_focuses() {
    FocusManager::get_instance().clear();

    CheckBox box;
    box.set_size({10, 1});
    box.update_layout(Rect::from_size(SCREEN));
    assert(!box.is_checked());

    std::vector<InputEvent> events{left_click({1, 0})};
    box.route_input(events);

    assert(box.is_checked());
    assert(box.is_focused());
    assert(events.empty());

    FocusManager::get_instance().clear();
}

void test_checkbox_keyboard_toggles() {
    FocusManager::get_instance().clear();

    CheckBox box;
    box.set_size({10, 1});
    box.update_layout(Rect::from_size(SCREEN));
    box.grab_focus();
    assert(box.is_focused());

    std::vector<InputEvent> space{key_press(Key::SPACE)};
    box.route_input(space);
    assert(box.is_checked());
    assert(space.empty());

    std::vector<InputEvent> enter{key_press(Key::ENTER)};
    box.route_input(enter);
    assert(!box.is_checked());
    assert(enter.empty());

    FocusManager::get_instance().clear();
}

void test_checkbox_signal_only_on_change() {
    CheckBox box;

    int changes = 0;
    bool last = true;
    box.connect_signal<bool>("toggled", [&](bool value) {
        ++changes;
        last = value;
    });

    box.set_checked(true);
    box.set_checked(true);
    box.toggle();

    assert(changes == 2);
    assert(!last);
}

void test_checkbox_draw_and_minimum() {
    CheckBox box;
    box.set_size({7, 1});

    // "[x]" is three columns; the text adds a space plus its own width.
    assert((box.get_minimum_size() == Vec2{3, 1}));

    CapturingTerminal terminal;
    ConsoleRenderer renderer(SCREEN, &terminal);
    box.update_layout(Rect::from_size(SCREEN));

    draw_checkbox(box, renderer);
    assert(terminal.glyph(0, 0) == L'[');
    assert(terminal.glyph(1, 0) == L' ');
    assert(terminal.glyph(2, 0) == L']');

    box.set_checked(true);
    draw_checkbox(box, renderer);
    assert(terminal.glyph(1, 0) == L'x');

    box.set_text("abc");
    assert((box.get_minimum_size() == Vec2{7, 1}));
    draw_checkbox(box, renderer);
    assert(terminal.glyph(3, 0) == L' ');
    assert(terminal.glyph(4, 0) == L'a');
    assert(terminal.glyph(5, 0) == L'b');
    assert(terminal.glyph(6, 0) == L'c');
}

void test_checkbox_custom_glyphs() {
    CheckBox box;
    box.set_size({3, 1});
    box.set_glyphs(U'X', U'.');

    CapturingTerminal terminal;
    ConsoleRenderer renderer(SCREEN, &terminal);
    box.update_layout(Rect::from_size(SCREEN));

    box.set_checked(true);
    draw_checkbox(box, renderer);
    assert(terminal.glyph(1, 0) == L'X');

    box.set_checked(false);
    draw_checkbox(box, renderer);
    assert(terminal.glyph(1, 0) == L'.');

    bool threw = false;
    try {
        box.set_glyphs(0, U' ');
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
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

    test_spinner_advances_with_delta();
    test_spinner_stop_and_start();
    test_spinner_empty_frames_draws_nothing();
    test_spinner_invalid_interval_falls_back();
    test_spinner_hidden_does_not_advance();
    test_spinner_minimum();

    test_checkbox_click_toggles_and_focuses();
    test_checkbox_keyboard_toggles();
    test_checkbox_signal_only_on_change();
    test_checkbox_draw_and_minimum();
    test_checkbox_custom_glyphs();

    std::cout << "basic_controls: all checks passed." << std::endl;
    return 0;
}
