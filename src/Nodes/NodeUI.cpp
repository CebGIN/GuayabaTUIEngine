#include <guayaba/Core/Color.hpp>
#include <guayaba/Nodes/NodeUI.hpp>

namespace guayaba {

void NodeUI::recalculate_size() {
    if (!text.empty()) {
        size.x = static_cast<int>(text[0].size());
        size.y = static_cast<int>(text.size());
    } else {
        size.x = 0;
        size.y = 0;
    }
}

NodeUI::NodeUI(const std::string& node_name, Vec2 node_position, std::vector<std::string> node_text) :
    Node2D(node_name, node_position), text(node_text),
    text_color(Color::from_name(Color::BLACK)),
    background_color(Color::from_name(Color::WHITE))
{
    recalculate_size();
}

NodeUI::NodeUI(const std::string& node_name, Vec2 node_position,
       const std::string& text_color_name, const std::string& background_color_name,
       std::vector<std::string> node_text) :
    Node2D(node_name, node_position), text(node_text),
    text_color(Color::from_name(text_color_name)),
    background_color(Color::from_name(background_color_name))
{
    recalculate_size();
}

void NodeUI::set_text(std::vector<std::string> new_text){
    text = new_text;
    recalculate_size();
}

std::vector<std::string> NodeUI::get_text(){
    return text;
}

NodeType NodeUI::get_type() const {
    return NodeType::NODEUI;
}

bool NodeUI::is_inside(const Vec2 point){
    Vec2 global_pos = get_global_position();
    return (point.x >= global_pos.x && point.x < global_pos.x + size.x &&
            point.y >= global_pos.y && point.y < global_pos.y + size.y);
}

void NodeUI::draw(ConsoleRenderer& renderer) {
    Vec2 global_pos = get_global_position();

    for (int y_offset = 0; y_offset < static_cast<int>(text.size()); ++y_offset) {
        renderer.put_string(global_pos + Vec2{0, y_offset}, text[y_offset], text_color, background_color);
    }
    Node::draw(renderer);
}

void NodeUI::change_text_color(const std::string& text_color_name){
    text_color = Color::from_name(text_color_name);
}

void NodeUI::change_background_color(const std::string& background_color_name){
    background_color = Color::from_name(background_color_name);
}

Vec2 NodeUI::get_size() const { return size; }

} // namespace guayaba