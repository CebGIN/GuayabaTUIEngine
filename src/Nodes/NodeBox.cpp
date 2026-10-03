#include <guayaba/Core/Color.hpp>
#include <guayaba/Nodes/NodeBox.hpp>

namespace guayaba {

NodeBox::NodeBox(const std::string& node_name, Vec2 node_position, Vec2 node_size, const std::string& background_color_name) : Node2D(node_name, node_position),
    size(node_size), background_color(Color::from_name(background_color_name)) {}

Vec2 NodeBox::get_size() const{
    return size;
}

void NodeBox::set_size(Vec2 new_size) {
    size = new_size;
}

void NodeBox::change_background_color(const std::string& background_color_name){
    background_color = Color::from_name(background_color_name);
}

void NodeBox::draw(ConsoleRenderer& renderer) {
    Vec2 global_pos = get_global_position();

    for (int j = 0; j < static_cast<int>(size.y); ++j){
        for(int i = 0; i < size.x; ++i){
            renderer.put_char(global_pos + Vec2{i, j}, L' ', 0, background_color);
        }
    }

    Node::draw(renderer);
}

} // namespace guayaba