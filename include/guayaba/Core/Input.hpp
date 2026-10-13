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
    Everything that happened during the last refresh, oldest first.

    Returns a vector by value and empties the one held here, so every event is
    delivered exactly once however many callers drain it. The frame loop hands
    the result to Node::route_input, which walks the tree and lets controls
    consume what they want; a scene that wants the raw events reads them first,
    or listens for "input" on the bus instead.

    Empty when there is no terminal, which is what a headless run gets.
*/
std::vector<InputEvent> drain_input_events();

/*
    Every input the engine knows about is also a bus signal, so a scene can react
    to a click instead of polling Input::l_click_just_pressed in on_process. All
    are emitted from the frame path, so a reactor sees the event in the same frame.

        bus().connect_script_signal("mouse_clicked", this, &MyScript::al_hacer_click);

    HOW THE PAYLOADS ARE SHAPED
    They are checked at the connect call, not at the emit: a reactor for
    "mouse_clicked" has to take (int, Vec2) and nothing else. Get the signature
    wrong and emit_signal says so rather than quietly not firing.

    WHAT IS AND IS NOT EMITTED
    mouse_clicked fires at most once per refresh, and reports 1 (left) if both
    buttons register on the same poll - the terminal hands over two booleans, not a
    queue of events. mouse_moved fires only when the position actually changed
    since the previous refresh, which means NOT on the first frame.

    "input" CARRIES EVERY EVENT, BEFORE ROUTING
    refresh_input() emits "input" once per InputEvent, before the tree gets them.
    A reactor with signature (const InputEvent&) sees every key press, wheel notch
    and mouse movement, whether or not a control later consumes it. This is the
    ambient view, for an observer.

    "unhandled_input" CARRIES WHAT ROUTING LEFT
    After Node::route_input has let controls consume events, the frame loop emits
    "unhandled_input" for whatever remains. A global shortcut wants this one, not
    "input": a key typed into a focused text field is consumed during routing and
    never reaches "unhandled_input", so the shortcut does not fire while typing.

    Scripts attached to a node get the same two opportunities without the bus:
    override Script::on_input (consumable, in the tree walk) or
    Script::on_unhandled_input (the fallback).
*/

} // namespace guayaba::Input

#endif // GUAYABA_CORE_INPUT_HPP