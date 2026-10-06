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

/*
    Every input the engine knows about is also a bus signal, so a scene can react
    to a click instead of polling Input::l_click_just_pressed in on_process. Both
    are emitted from refresh_input(), which the frame loop calls before
    process_scene(), so a reactor sees the event in the same frame.

        bus().connect_script_signal("mouse_clicked", this, &MyScript::al_hacer_click);

    HOW THE PAYLOADS ARE SHAPED
    They are checked at the connect call, not at the emit: a reactor for
    "mouse_clicked" has to take (int, Vec2) and nothing else. Get the signature
    wrong and emit_signal says so rather than quietly not firing.

    WHAT IS AND IS NOT EMITTED
    mouse_clicked fires at most once per refresh, and reports 1 (left) if both
    buttons register on the same poll - the terminal hands over two booleans, not a
    queue of events. mouse_moved fires only when the position actually changed
    since the previous refresh, which means NOT on the first frame: there is no
    previous position yet, and a listener that highlights or follows the pointer
    should not be handed a movement that never happened.

    WHY THERE IS NO KEY SIGNAL
    There is no key signal because the engine has no key input: IOSTerminal exposes
    poll_events, the mouse and blocking text reads, and nothing else. Adding one
    means adding a key poll to IOSTerminal and both backends first, and at that
    point "key_pressed" belongs here beside these two.
*/

} // namespace guayaba::Input

#endif // GUAYABA_CORE_INPUT_HPP