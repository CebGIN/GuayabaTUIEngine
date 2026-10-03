#include <guayaba/Nodes/Node.hpp>

namespace guayaba {

void Node::enter_tree() {
    in_tree = true;
    at_enter_tree();
    for (const auto& child : children) {
        child->enter_tree();
    }
}

void Node::exit_tree() {
    for (const auto& child : children) {
        child->exit_tree();
    }
    at_exit_tree();
    in_tree = false;
}

Node::Node(const std::string& node_name) : name(node_name) {
    process = [](double){}; at_enter_tree = [](){}; at_exit_tree = [](){};
}

Node::~Node() {}

void Node::set_parent(std::shared_ptr<Node> parent_node) {
    parent = parent_node;
}

std::shared_ptr<Node> Node::get_parent() const {
    return parent.lock();
}

bool Node::is_root() const {
    return parent.expired() || !parent.lock();
}

void Node::add_child(std::shared_ptr<Node> child_node) {
    if (child_node) {
        children.push_back(child_node);
        child_node->set_parent(shared_from_this());
        if (in_tree) child_node->enter_tree();
    }
}

size_t Node::get_child_count() const {
    return children.size();
}

std::shared_ptr<Node> Node::get_child(size_t index) {
    if (index < children.size()) {
        return children[index];
    } else {
        throw std::out_of_range("Node::get_child(index): Index out of bounds. en: " + name);
    }
}

std::shared_ptr<const Node> Node::get_child(size_t index) const {
    if (index < children.size()) {
        return children[index];
    } else {
        throw std::out_of_range("Node::get_child(index): Index out of bounds.");
    }
}

const std::string& Node::get_name() const {
    return name;
}

NodeType Node::get_type() const {
    return NodeType::NODE;
}

void Node::update(double delta_time) {
    process(delta_time);
    for (const auto& child : children) {
        child->update(delta_time);
    }
}

void Node::set_process_function(std::function<void(double)> func) {
    if (func) { process = func; }
    else { process = [](double){}; }
}

void Node::set_at_enter_function(std::function<void()> func) {
    if (func) { at_enter_tree = func; }
    else { at_enter_tree = [](){}; }
}

void Node::set_at_exit_function(std::function<void()> func) {
    if (func) { at_exit_tree = func; }
    else { at_exit_tree = [](){}; }
}

void Node::draw(ConsoleRenderer& renderer) {
    for (const auto& child : children) {
        child->draw(renderer);
    }
}

void Node::invalidate_cache_recursively(){
    for (const auto& child : children) {
        child->invalidate_cache_recursively();
    }
}

} // namespace guayaba