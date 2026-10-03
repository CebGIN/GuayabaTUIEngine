#include <iostream>

#include <guayaba/Core/Input.hpp>

namespace guayaba::Input {

IOSTerminal* g_terminal = nullptr;

Vec2 mouse_pos = {0, 0};
bool l_click = false;
bool r_click = false;
bool l_click_prev = false;
bool r_click_prev = false;
bool l_click_just_pressed = false;
bool r_click_just_pressed = false;

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
}

std::string get_line_input(Vec2 position) {
    if (g_terminal) return g_terminal->get_line_input(position);
    std::string input;
    std::getline(std::cin, input);
    return input;
}

} // namespace guayaba::Input