// Headless checks for NODE-06: SpinBox formatting, live parsing, clamp/wrap,
// arrows, PageUp/PageDown, arrow clicks and read-only mode.

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include <guayaba/Guayaba.hpp>

using namespace guayaba;

namespace {

constexpr Vec2 SCREEN{20, 4};

InputEvent key_event(Key key) {
    InputEvent event;
    event.type = InputEvent::Type::KEY;
    event.pressed = true;
    event.key = key;
    return event;
}

InputEvent text_event(char32_t codepoint) {
    InputEvent event;
    event.type = InputEvent::Type::KEY;
    event.pressed = true;
    event.text = codepoint;
    return event;
}

InputEvent left_click(Vec2 pos) {
    InputEvent event;
    event.type = InputEvent::Type::MOUSE_BUTTON;
    event.pressed = true;
    event.mouse_button = MouseButton::LEFT;
    event.mouse_pos = pos;
    return event;
}

void send(SpinBox& spin, InputEvent event) {
    std::vector<InputEvent> events{event};
    spin.route_input(events);
}

void reset_focus() {
    FocusManager::get_instance().clear();
}

void test_format_and_decimals() {
    SpinBox spin;
    spin.set_range(0, 1000);

    spin.set_value(3.14159);  // decimals 0: stored rounded to 3
    assert(spin.get_text() == "3");

    spin.set_decimals(2);
    assert(spin.get_text() == "3.00");

    spin.set_value(3.14159);
    assert(spin.get_text() == "3.14");
    assert(spin.get_value() == 3.14);

    spin.set_value(2.25);
    assert(spin.get_text() == "2.25");
    assert(spin.get_value() == 2.25);
}

void test_clamp() {
    SpinBox spin;
    spin.set_range(0, 10);

    spin.set_value(-5);
    assert(spin.get_value() == 0);
    assert(spin.get_text() == "0");

    spin.set_value(50);
    assert(spin.get_value() == 10);
    assert(spin.get_text() == "10");
}

void test_wrap() {
    SpinBox spin;
    spin.set_range(0, 10);
    spin.set_step(1);
    spin.set_wrap(true);
    spin.set_value(10);
    spin.grab_focus();

    // Up from the maximum wraps to the minimum.
    send(spin, key_event(Key::UP));
    assert(spin.get_value() == 0);

    // Down from the minimum wraps to the maximum.
    send(spin, key_event(Key::DOWN));
    assert(spin.get_value() == 10);

    reset_focus();
}

void test_step_and_page_keys() {
    SpinBox spin;
    spin.set_range(0, 100);
    spin.set_step(2);
    spin.set_value(10);
    spin.grab_focus();

    send(spin, key_event(Key::UP));
    assert(spin.get_value() == 12);

    send(spin, key_event(Key::DOWN));
    assert(spin.get_value() == 10);

    send(spin, key_event(Key::PAGE_UP));
    assert(spin.get_value() == 30);

    send(spin, key_event(Key::PAGE_DOWN));
    assert(spin.get_value() == 10);

    reset_focus();
}

void test_arrow_click() {
    SpinBox spin;
    spin.set_range(0, 100);
    spin.set_value(5);
    spin.set_size({10, 2});
    spin.update_layout(Rect::from_size({10, 2}));

    // The text area is 8 wide; the two reserved columns are x = 8 and 9.
    send(spin, left_click({8, 0}));
    assert(spin.get_value() == 6);
    assert(spin.is_focused());

    send(spin, left_click({8, 1}));
    assert(spin.get_value() == 5);

    reset_focus();
}

void test_live_edit_and_enter() {
    SpinBox spin;
    spin.set_range(0, 100);
    spin.set_value(5);  // text "5", caret at the end
    spin.grab_focus();

    int changes = 0;
    double last = -1.0;
    spin.connect_signal<double>("value_changed", [&](double value) {
        ++changes;
        last = value;
    });

    // Typing appends and updates the value without reformatting.
    send(spin, text_event(U'7'));
    assert(spin.get_text() == "57");
    assert(spin.get_value() == 57);
    assert(changes == 1);
    assert(last == 57.0);

    // Enter rewrites the text with the configured format.
    send(spin, key_event(Key::ENTER));
    assert(spin.get_text() == "57");
    assert(spin.get_value() == 57);

    reset_focus();
}

void test_enter_clamps() {
    SpinBox spin;
    spin.set_range(0, 100);
    spin.set_value(5);
    spin.grab_focus();

    send(spin, text_event(U'9'));
    send(spin, text_event(U'9'));
    assert(spin.get_text() == "599");
    assert(spin.get_value() == 100);  // clamped live

    send(spin, key_event(Key::ENTER));
    assert(spin.get_text() == "100");

    reset_focus();
}

void test_invalid_text_and_focus_loss() {
    SpinBox spin;
    spin.set_range(0, 100);
    spin.set_value(5);
    spin.grab_focus();

    // A letter makes the text invalid; the value stays put.
    send(spin, text_event(U'a'));
    assert(spin.get_text() == "5a");
    assert(spin.get_value() == 5);

    // Enter rewrites the value, discarding the invalid suffix.
    send(spin, key_event(Key::ENTER));
    assert(spin.get_text() == "5");

    // Losing the focus does the same after a live edit.
    spin.set_text("7.9");
    assert(spin.get_value() == 8);  // snapped to 0 decimals
    assert(spin.get_text() == "7.9");

    reset_focus();
    assert(spin.get_text() == "8");
}

void test_read_only_degenerate_range() {
    SpinBox spin;
    spin.set_range(5, 5);
    spin.set_value(100);

    assert(spin.get_value() == 5);
    assert(spin.get_text() == "5");

    spin.grab_focus();
    send(spin, key_event(Key::UP));
    assert(spin.get_value() == 5);

    send(spin, text_event(U'9'));
    assert(spin.get_text() == "5");

    reset_focus();
}

void test_normalization_and_minimum() {
    SpinBox spin;

    spin.set_decimals(-3);
    assert(spin.get_decimals() == 0);

    spin.set_decimals(99);
    assert(spin.get_decimals() == 15);

    spin.set_step(0.0);
    assert(spin.get_step() > 0.0);

    spin.set_step(-2.0);
    assert(spin.get_step() > 0.0);

    // The two arrow columns come from the StyleBox's right margin, so the
    // inherited minimum already counts them.
    LineEdit plain;
    SpinBox box;
    assert(box.get_minimum_size().x == plain.get_minimum_size().x + 2);
    assert(box.get_minimum_size().y == plain.get_minimum_size().y);
}

} // namespace

int main() {
    test_format_and_decimals();
    test_clamp();
    test_wrap();
    test_step_and_page_keys();
    test_arrow_click();
    test_live_edit_and_enter();
    test_enter_clamps();
    test_invalid_text_and_focus_loss();
    test_read_only_degenerate_range();
    test_normalization_and_minimum();

    std::cout << "spinbox: all checks passed." << std::endl;
    return 0;
}
