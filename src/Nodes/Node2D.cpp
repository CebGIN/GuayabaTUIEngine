#include <guayaba/Core/NodeRef.hpp>
#include <guayaba/Nodes/Node2D.hpp>

namespace guayaba {

Node2D::Node2D(const std::string& node_name, Vec2 node_position)
    : Node(node_name), position(node_position){}

void Node2D::invalidate_cache_recursively() {
    if (!this->is_cached_position_valid) return;
    this->is_cached_position_valid = false;
    Node::invalidate_cache_recursively();
}

Vec2 Node2D::get_local_position() const {
    return position;
}

void Node2D::set_local_position(Vec2 new_position){
    if(new_position == position) return;
    position = new_position;
    invalidate_cache_recursively();
}

NodeType Node2D::get_type() const {
    return NodeType::NODE2D;
}

Vec2 Node2D::get_global_position() const {
    if (is_cached_position_valid) return cached_global_position;
    Vec2 global_pos = position;

    if (const NodeRef<Node2D> parent2D = get_parent_of_type<Node2D>()) {
        global_pos = global_pos + parent2D->get_global_position();
    }

    cached_global_position = global_pos;
    is_cached_position_valid = true;
    return global_pos;
}

void Node2D::set_global_position(Vec2 new_global_position) {
    set_local_position(((new_global_position + get_local_position()) - get_global_position()));
    this->cached_global_position = new_global_position;
    this->is_cached_position_valid = true;
}

} // namespace guayaba