#ifndef GUAYABA_CORE_INPUT_HPP
#define GUAYABA_CORE_INPUT_HPP

#include <string>

#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Terminal/IOSTerminal.hpp>

namespace guayaba::Input {

extern IOSTerminal* g_terminal;

extern Vec2 mouse_pos;
extern bool l_click;
extern bool r_click;
extern bool l_click_prev;
extern bool r_click_prev;
extern bool l_click_just_pressed;
extern bool r_click_just_pressed;

void refresh_input();

template<typename T>
T get_typed_input(Vec2 position = {0, 0}){
    if (g_terminal) return g_terminal->get_typed_input<T>(position);
    T input;
    std::cin >> input;
    return input;
}

std::string get_line_input(Vec2 position = {0, 0});

} // namespace guayaba::Input

#endif // GUAYABA_CORE_INPUT_HPP