#include <guayaba/Core/Color.hpp>
#include <guayaba/Nodes/NodeSQ.hpp>

namespace guayaba {

NodeSQ::NodeSQ(const std::string& node_name, Vec2 node_position, Vec2 node_size,
    const std::string& text_color_name, const std::string& background_color_name) : Node2D(node_name, node_position),
    size(node_size), text_color(Color::from_name(text_color_name)), background_color(Color::from_name(background_color_name)) {}

Vec2 NodeSQ::get_size() const{
    return size;
}

void NodeSQ::set_size(Vec2 new_size) {
    size = new_size;
}

void NodeSQ::change_text_color(const std::string& text_color_name){
    text_color = Color::from_name(text_color_name);
}

void NodeSQ::change_background_color(const std::string& background_color_name){
    background_color = Color::from_name(background_color_name);
}

void NodeSQ::draw(ConsoleRenderer& renderer) {
    Vec2 global_pos = get_global_position();

    for (int j = 1; j < static_cast<int>(size.y - 1); ++j){
        renderer.put_char(global_pos + Vec2{0, j}, L'|', text_color, background_color);
    }
    for (int j = 1; j < static_cast<int>(size.y - 1); ++j){
        renderer.put_char(global_pos + Vec2{static_cast<int>(size.x - 1), j}, L'|', text_color, background_color);
    }

    for(int i = 0; i < size.x; ++i){
        renderer.put_char(global_pos + Vec2{i, 0}, L'-', text_color, background_color);
    }
    for(int i = 0; i < size.x; ++i){
        renderer.put_char(global_pos + Vec2{i, static_cast<int>(size.y - 1)}, L'-', text_color, background_color);
    }

    Node::draw(renderer);
}

} // namespace guayaba