#include <guayaba/Core/Color.hpp>
#include <guayaba/Nodes/NodeTextMatrix.hpp>

namespace guayaba {

void NodeTextMatrix::recalculate_size() {
    if (!text.empty()) {
        size.x = static_cast<int>(text[0].size());
        size.y = static_cast<int>(text.size());
    } else {
        size.x = 0;
        size.y = 0;
    }
}

NodeTextMatrix::NodeTextMatrix(const std::string& node_name) : Node2D(node_name) {}


void NodeTextMatrix::set_text(std::vector<std::string> new_text){
    text = new_text;
    recalculate_size();
}

std::vector<std::string> NodeTextMatrix::get_text(){
    return text;
}

bool NodeTextMatrix::is_inside(const Vec2 point){
    Vec2 global_pos = get_global_position();
    return (point.x >= global_pos.x && point.x < global_pos.x + size.x &&
            point.y >= global_pos.y && point.y < global_pos.y + size.y);
}

void NodeTextMatrix::draw(ConsoleRenderer& renderer, const Rect& clip) {
    Vec2 global_pos = get_global_position();

    renderer.put_text(global_pos, text, text_color, background_color, clip);
    Node::draw(renderer, clip);
}

void NodeTextMatrix::change_text_color(const std::string& text_color_name){
    text_color = Color::from_name(text_color_name);
}

void NodeTextMatrix::change_background_color(const std::string& background_color_name){
    background_color = Color::from_name(background_color_name);
}

Vec2 NodeTextMatrix::get_size() const { return size; }

} // namespace guayaba