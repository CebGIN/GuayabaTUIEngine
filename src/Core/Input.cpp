#include <iostream>
#include <utility>

#include <guayaba/Core/Input.hpp>
#include <guayaba/Core/SignalBus.hpp>

namespace guayaba::Input {

IOSTerminal* g_terminal = nullptr;

Vec2 mouse_pos = {0, 0};
bool l_click = false;
bool r_click = false;
bool l_click_prev = false;
bool r_click_prev = false;
bool l_click_just_pressed = false;
bool r_click_just_pressed = false;

namespace {

// The position at the previous refresh, so mouse_moved reports a real move. The
// first refresh only records the position and emits nothing.
Vec2 previous_mouse_pos = {0, 0};
bool has_previous_mouse = false;

// This frame's events, waiting to be drained. Held here so refresh_input can emit
// "input" for each before the tree walk consumes it.
std::vector<InputEvent> frame_events;

} // namespace

void refresh_input() {
    if (!g_terminal) {
        frame_events.clear();
        return;
    }

    g_terminal->poll_events();

    l_click_prev = l_click;
    r_click_prev = r_click;

    l_click = g_terminal->is_left_click_pressed();
    r_click = g_terminal->is_right_click_pressed();

    l_click_just_pressed = g_terminal->is_left_click_just_pressed();
    r_click_just_pressed = g_terminal->is_right_click_just_pressed();

    mouse_pos = g_terminal->get_mouse_position();

    // Taken now so a draining reactor gets the same events the tree walk will.
    frame_events = g_terminal->take_input_events();

    // Emitted before process_scene(), so a reactor sees the click in the same
    // frame the scripts update in. The guard is is_declared, not
    // has_signal<int, Vec2>: is_declared ignores the signature, so a mismatch
    // throws from emit_signal instead of silently never firing.
    SignalBus& bus = SignalBus::get_instance();

    // At most one per frame; left wins if both register on the same poll.
    if ((l_click_just_pressed || r_click_just_pressed)
        && bus.is_declared("mouse_clicked")) {
        // 1 and 2 rather than an enum, so a reactor can compare without this header.
        bus.emit_signal("mouse_clicked", l_click_just_pressed ? 1 : 2, mouse_pos);
    }

    if (has_previous_mouse
        && (mouse_pos.x != previous_mouse_pos.x || mouse_pos.y != previous_mouse_pos.y)
        && bus.is_declared("mouse_moved")) {
        bus.emit_signal("mouse_moved", mouse_pos);
    }
    previous_mouse_pos = mouse_pos;
    has_previous_mouse = true;

    // Every event, emitted before routing so an observer sees the same order the
    // controls will.
    if (bus.is_declared("input")) {
        for (const InputEvent& event : frame_events) {
            bus.emit_signal("input", event);
        }
    }
}

std::vector<InputEvent> drain_input_events() {
    // Swapped out so the caller takes the events and this namespace keeps none.
    std::vector<InputEvent> drained;
    drained.swap(frame_events);
    return drained;
}

std::string get_line_input(Vec2 position) {
    if (g_terminal) return g_terminal->get_line_input(position);
    std::string input;
    std::getline(std::cin, input);
    return input;
}

} // namespace guayaba::Input