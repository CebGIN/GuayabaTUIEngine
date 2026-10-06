#include <guayaba/Core/Input.hpp>
#include <guayaba/Nodes/NodeButton.hpp>

namespace guayaba {

NodeButton::NodeButton(const std::string& node_name, Vec2 node_position, std::string text_color_name, std::string background_color_name, std::vector<std::string> node_text) :
    NodeUI(node_name, node_position, text_color_name, background_color_name, node_text) {
    // Pre-declared, so that has_signal("clicked") is a real answer from the start
    // and handle_click() costs a map lookup rather than an allocation on a button
    // nobody is listening to. Declared void() because a click carries nothing this
    // button can know: the reactor can read Input::mouse_pos if it needs where.
    declare_signal("clicked");
}

void NodeButton::update(double delta_time) {
    update_hover(Input::mouse_pos);
    handle_click();
    Node::update(delta_time);
}

NodeType NodeButton::get_type() const {
    return NodeType::NODEBUTTON;
}

bool NodeButton::is_hovered(){
    return hovered;
}

void NodeButton::update_hover(const Vec2 mouse_global_pos) {
    hovered = is_inside(mouse_global_pos);
}

void NodeButton::handle_click() {
    if (!(Input::l_click_just_pressed && hovered)) return;

    // The signal is the decoupled route: a reactor elsewhere connects to it without
    // anyone having to keep a handle to this button. on_click below is the older,
    // equally valid one-callback-per-button route, and both fire.
    emit_signal("clicked");

    if (on_click) on_click();
}

void NodeButton::set_on_click(std::function<void()> callback) {
    on_click = callback;
}

void NodeButton::draw(ConsoleRenderer& renderer) {
    Vec2 global_pos = get_global_position();
    uint16_t current_fg = text_color;
    uint16_t current_bg = background_color;

    if (hovered) {
        // Swap foreground and background on hover
        current_fg = background_color;
        current_bg = text_color;
    }

    for (int y_offset = 0; y_offset < static_cast<int>(text.size()); ++y_offset) {
        Vec2 current_line_global_pos = (global_pos + Vec2{0, y_offset});
        std::string line = text[y_offset];
        renderer.put_string(current_line_global_pos, line, current_fg, current_bg);
    }
    Node::draw(renderer);
}

} // namespace guayaba