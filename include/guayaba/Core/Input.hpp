#ifndef GUAYABA_CORE_INPUT_HPP
#define GUAYABA_CORE_INPUT_HPP

#include <string>
#include <vector>

#include <guayaba/Core/InputEvent.hpp>
#include <guayaba/Core/Key.hpp>
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

/*
    All events from the last refresh, oldest first. Returns a vector by value and
    empties the one held here, so each event is delivered once. The frame loop
    hands the result to Node::route_input. Empty when there is no terminal.
*/
std::vector<InputEvent> drain_input_events();

/*
    Every input the engine knows about is also a bus signal, emitted from the
    frame path so a reactor sees the event in the same frame.

        bus().connect_script_signal("mouse_clicked", this, &MyScript::al_hacer_click);

    mouse_clicked: (int button, Vec2 pos), at most once per refresh; 1 for left,
    and left wins if both buttons register on the same poll. mouse_moved: (Vec2
    pos), only when the position changed, so not on the first frame.

    "input" carries every InputEvent before routing; "unhandled_input" carries
    what routing left. A global shortcut wants the latter, since a key consumed by
    a focused field never reaches it. Signatures are checked at the connect call.

    Scripts get the same two hooks without the bus: Script::on_input (consumable)
    and Script::on_unhandled_input.
*/

} // namespace guayaba::Input

#endif // GUAYABA_CORE_INPUT_HPP