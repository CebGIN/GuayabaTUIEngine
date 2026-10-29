// Headless checks for NODE-11: SplitContainer layout in both orientations, the
// free-space ratio, clamp/wrap at the edges, keyboard, dragging with capture and
// the divider StyleBox.

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include <guayaba/Guayaba.hpp>

using namespace guayaba;

namespace {

constexpr Vec2 SCREEN{20, 5};

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

    uint16_t background(int x, int y) const {
        return buffer[static_cast<std::size_t>(y) * size.x + x].background;
    }
};

InputEvent key_event(Key key) {
    InputEvent event;
    event.type = InputEvent::Type::KEY;
    event.pressed = true;
    event.key = key;
    return event;
}

InputEvent mouse_press(Vec2 pos) {
    InputEvent event;
    event.type = InputEvent::Type::MOUSE_BUTTON;
    event.pressed = true;
    event.mouse_button = MouseButton::LEFT;
    event.mouse_pos = pos;
    return event;
}

InputEvent mouse_release(Vec2 pos) {
    InputEvent event;
    event.type = InputEvent::Type::MOUSE_BUTTON;
    event.pressed = false;
    event.mouse_button = MouseButton::LEFT;
    event.mouse_pos = pos;
    return event;
}

InputEvent mouse_motion(Vec2 pos) {
    InputEvent event;
    event.type = InputEvent::Type::MOUSE_MOTION;
    event.mouse_pos = pos;
    return event;
}

void send(SplitContainer& split, InputEvent event) {
    std::vector<InputEvent> events{event};
    split.route_input(events);
}

void reset_focus() {
    FocusManager::get_instance().clear();
}

// Two full-rect panels, so their resolved rects are exactly the cells.
void add_panels(SplitContainer& split, NodeRef<Control>& first, NodeRef<Control>& second) {
    first = split.add_child<Control>("first");
    second = split.add_child<Control>("second");
    first->set_anchor_preset(AnchorPreset::FullRect);
    second->set_anchor_preset(AnchorPreset::FullRect);
}

void test_vertical_layout() {
    SplitContainer split;
    split.set_size(SCREEN);
    split.set_min_first(2);
    split.set_min_second(3);
    split.set_divider_width(1);
    split.set_split_ratio(0.5f);

    NodeRef<Control> first;
    NodeRef<Control> second;
    add_panels(split, first, second);

    split.update_layout(Rect::from_size(SCREEN));

    // free = 20 - 1 - 2 - 3 = 14; first = 2 + round(7) = 9; second = 20 - 9 - 1.
    assert((first->get_rect() == Rect{Vec2{0, 0}, Vec2{9, 5}}));
    assert((second->get_rect() == Rect{Vec2{10, 0}, Vec2{10, 5}}));
}

void test_horizontal_layout() {
    SplitContainer split;
    split.set_orientation(SplitOrientation::HorizontalDivider);
    split.set_size({20, 10});
    split.set_min_first(2);
    split.set_min_second(3);
    split.set_divider_width(1);
    split.set_split_ratio(0.5f);

    NodeRef<Control> first;
    NodeRef<Control> second;
    add_panels(split, first, second);

    split.update_layout(Rect::from_size({20, 10}));

    // free = 10 - 1 - 2 - 3 = 4; first = 2 + 2 = 4; second = 10 - 4 - 1.
    assert((first->get_rect() == Rect{Vec2{0, 0}, Vec2{20, 4}}));
    assert((second->get_rect() == Rect{Vec2{0, 5}, Vec2{20, 5}}));
}

void test_minimum_size() {
    SplitContainer split;
    split.set_min_first(4);
    split.set_min_second(6);
    split.set_divider_width(2);

    NodeRef<Control> first;
    NodeRef<Control> second;
    add_panels(split, first, second);
    first->set_custom_minimum_size({0, 3});
    second->set_custom_minimum_size({0, 1});

    // Split axis: both minimums plus the divider. Cross axis: the tallest panel.
    assert((split.get_minimum_size() == Vec2{12, 3}));
}

void test_ratio_clamp_and_signal() {
    SplitContainer split;

    int changes = 0;
    float last = -1.0f;
    split.connect_signal<float>("split_ratio_changed", [&](float ratio) {
        ++changes;
        last = ratio;
    });

    split.set_split_ratio(0.25f);
    assert(changes == 1);
    assert(last == 0.25f);

    split.set_split_ratio(0.25f);
    assert(changes == 1);

    split.set_split_ratio(2.0f);
    assert(split.get_split_ratio() == 1.0f);
    assert(changes == 2);

    split.set_split_ratio(-1.0f);
    assert(split.get_split_ratio() == 0.0f);
    assert(changes == 3);
}

void test_keyboard_moves_one_cell() {
    reset_focus();

    SplitContainer split;
    split.set_size(SCREEN);
    split.set_divider_width(1);
    split.set_split_ratio(0.5f);

    NodeRef<Control> first;
    NodeRef<Control> second;
    add_panels(split, first, second);

    split.update_layout(Rect::from_size(SCREEN));
    assert(first->get_rect().size.x == 10);

    split.grab_focus();
    send(split, key_event(Key::RIGHT));
    split.update_layout(Rect::from_size(SCREEN));
    assert(first->get_rect().size.x == 11);

    send(split, key_event(Key::LEFT));
    split.update_layout(Rect::from_size(SCREEN));
    assert(first->get_rect().size.x == 10);

    reset_focus();
}

void test_drag_with_capture() {
    SplitContainer split;
    split.set_size(SCREEN);
    split.set_divider_width(1);
    split.set_split_ratio(0.5f);

    NodeRef<Control> first;
    NodeRef<Control> second;
    add_panels(split, first, second);
    split.update_layout(Rect::from_size(SCREEN));

    // The divider sits at x = 10.
    send(split, mouse_press({10, 2}));
    assert(split.has_mouse_capture());

    // Motion far outside the rect still reaches the captor; the ratio clamps.
    send(split, mouse_motion({30, 2}));
    split.update_layout(Rect::from_size(SCREEN));
    assert(first->get_rect().size.x == 19);

    send(split, mouse_release({30, 2}));
    assert(!split.has_mouse_capture());
}

void test_child_count() {
    SplitContainer split;
    split.set_size(SCREEN);
    split.set_divider_width(1);

    NodeRef<Control> only = split.add_child<Control>("only");
    only->set_anchor_preset(AnchorPreset::FullRect);

    // One panel: nothing is placed.
    split.update_layout(Rect::from_size(SCREEN));
    assert(only->get_rect().size.x == 0);

    NodeRef<Control> second = split.add_child<Control>("second");
    NodeRef<Control> extra = split.add_child<Control>("extra");
    second->set_anchor_preset(AnchorPreset::FullRect);
    extra->set_anchor_preset(AnchorPreset::FullRect);

    split.update_layout(Rect::from_size(SCREEN));

    // The first two are the panels; the third is ignored.
    assert(only->get_rect().size.x > 0);
    assert(second->get_rect().size.x > 0);
    assert(extra->get_rect().size.x == 0);
}

void test_hidden_child_is_not_a_panel() {
    SplitContainer split;
    split.set_size(SCREEN);
    split.set_divider_width(1);

    NodeRef<Control> first = split.add_child<Control>("first");
    NodeRef<Control> second = split.add_child<Control>("second");
    NodeRef<Control> third = split.add_child<Control>("third");
    first->set_anchor_preset(AnchorPreset::FullRect);
    second->set_anchor_preset(AnchorPreset::FullRect);
    third->set_anchor_preset(AnchorPreset::FullRect);

    first->set_visible(false);
    split.update_layout(Rect::from_size(SCREEN));

    // The visible pair is second and third.
    assert(first->get_rect().size.x == 0);
    assert(second->get_rect().size.x > 0);
    assert(third->get_rect().size.x > 0);
}

void test_divider_stylebox_is_optional() {
    SplitContainer split;
    split.set_size(SCREEN);
    split.set_divider_width(1);
    split.set_split_ratio(0.5f);

    NodeRef<Control> first;
    NodeRef<Control> second;
    add_panels(split, first, second);
    split.update_layout(Rect::from_size(SCREEN));

    CapturingTerminal terminal;
    ConsoleRenderer renderer(SCREEN, &terminal);

    split.draw(renderer, Rect::from_size(SCREEN));
    renderer.present();
    // Empty StyleBox: the divider column is left blank.
    assert(terminal.glyph(10, 0) == L' ');

    split.get_divider_stylebox().set_background_color(4);
    split.draw(renderer, Rect::from_size(SCREEN));
    renderer.present();
    assert(terminal.background(10, 0) == 4);
}

} // namespace

int main() {
    test_vertical_layout();
    test_horizontal_layout();
    test_minimum_size();
    test_ratio_clamp_and_signal();
    test_keyboard_moves_one_cell();
    test_drag_with_capture();
    test_child_count();
    test_hidden_child_is_not_a_panel();
    test_divider_stylebox_is_optional();

    std::cout << "split_container: all checks passed." << std::endl;
    return 0;
}
