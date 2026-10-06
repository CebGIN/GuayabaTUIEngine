#include <iostream>

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

// What mouse_pos was at the previous refresh, so mouse_moved means "the pointer
// went somewhere" rather than "a frame happened". Not a global on purpose: nothing
// outside refresh_input() may read it.
Vec2 last_emitted_mouse_pos = {0, 0};
bool has_emitted_mouse = false;

} // namespace

void refresh_input() {
    if (!g_terminal) return;
    g_terminal->poll_events();

    l_click_prev = l_click;
    r_click_prev = r_click;

    l_click = g_terminal->is_left_click_pressed();
    r_click = g_terminal->is_right_click_pressed();

    l_click_just_pressed = g_terminal->is_left_click_just_pressed();
    r_click_just_pressed = g_terminal->is_right_click_just_pressed();

    mouse_pos = g_terminal->get_mouse_position();

    // Emitted from here, which the frame loop already calls before process_scene():
    // a reactor therefore sees the click in the same frame the scripts update in.
    //
    // Both are on the bus rather than on a node because the terminal is not a node,
    // and both are checked at the connect call even though the bus erases the
    // arguments: a reactor for "mouse_clicked" has to take (int, Vec2).
    //
    // has_signal is what makes this cheap. Without it, every frame would declare a
    // signal nobody declared and walk an empty reactor list.
    SignalBus& bus = SignalBus::get_instance();

    if (l_click_just_pressed || r_click_just_pressed) {
        if (bus.has_signal<int, Vec2>("mouse_clicked")) {
            // 1 and 2 rather than an enum: a reactor can compare them without this
            // header, and a mouse with a middle button can be added later without
            // renumbering.
            bus.emit_signal("mouse_clicked", l_click_just_pressed ? 1 : 2, mouse_pos);
        }
    }

    if (!has_emitted_mouse || mouse_pos.x != last_emitted_mouse_pos.x
        || mouse_pos.y != last_emitted_mouse_pos.y) {
        if (bus.has_signal<Vec2>("mouse_moved")) {
            bus.emit_signal("mouse_moved", mouse_pos);
        }
        last_emitted_mouse_pos = mouse_pos;
        has_emitted_mouse = true;
    }
}

std::string get_line_input(Vec2 position) {
    if (g_terminal) return g_terminal->get_line_input(position);
    std::string input;
    std::getline(std::cin, input);
    return input;
}

} // namespace guayaba::Input