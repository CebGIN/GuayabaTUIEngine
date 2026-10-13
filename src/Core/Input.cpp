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

// The pointer position at the previous refresh, so mouse_moved means "the pointer
// went somewhere" rather than "a frame happened". File-local on purpose: nothing
// outside refresh_input() may read it.
//
// The first refresh only records the position, and emits nothing. There is no
// "previous" to compare against yet, so emitting would be reporting a movement
// that never happened - and the first thing a listener does with a mouse position
// is usually something visible.
Vec2 previous_mouse_pos = {0, 0};
bool has_previous_mouse = false;

// This frame's events, waiting to be drained. Held here rather than taken
// straight from the terminal so refresh_input can emit "input" for each of them
// before the tree walk gets a chance to consume one.
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

    // Taken from the terminal before anything can react, so a reactor that drains
    // gets the same frame's events the tree walk will.
    frame_events = g_terminal->take_input_events();

    // Emitted from here, which the frame loop already calls before process_scene():
    // a reactor therefore sees the click in the same frame the scripts update in.
    //
    // Both are on the bus rather than on a node because the terminal is not a node.
    // Both are checked at the connect call even though the bus erases the arguments:
    // a reactor for "mouse_clicked" has to take (int, Vec2).
    //
    // The guard is is_declared and NOT has_signal<int, Vec2>(...). is_declared
    // ignores the signature, which is what an emitter needs: has_signal is also
    // false when the name exists with a different signature, and using that as an
    // emit guard would turn one wrong guess at connect time into a signal that
    // silently stops firing for the rest of the process. With is_declared, a
    // mismatched reactor gets the throw from emit_signal instead of the quiet.
    SignalBus& bus = SignalBus::get_instance();

    // At most one per frame, and left wins if both somehow register on the same
    // poll: the terminal reports two booleans, not a queue of events.
    if ((l_click_just_pressed || r_click_just_pressed)
        && bus.is_declared("mouse_clicked")) {
        // 1 and 2 rather than an enum: a reactor can compare them without this
        // header, and a mouse with a middle button can be added later without
        // renumbering.
        bus.emit_signal("mouse_clicked", l_click_just_pressed ? 1 : 2, mouse_pos);
    }

    if (has_previous_mouse
        && (mouse_pos.x != previous_mouse_pos.x || mouse_pos.y != previous_mouse_pos.y)
        && bus.is_declared("mouse_moved")) {
        bus.emit_signal("mouse_moved", mouse_pos);
    }
    previous_mouse_pos = mouse_pos;
    has_previous_mouse = true;

    // Every event, not just the summary the two signals above provide. Emitted
    // before routing so an observer sees the same ordering the controls will, and
    // so a reactor cannot starve a control of an event - this loop holds its own
    // copy of each event.
    if (bus.is_declared("input")) {
        for (const InputEvent& event : frame_events) {
            bus.emit_signal("input", event);
        }
    }
}

std::vector<InputEvent> drain_input_events() {
    // Moved out rather than copied: the caller gets this frame's events and this
    // namespace keeps an empty vector, so two drains cannot both act on one click.
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