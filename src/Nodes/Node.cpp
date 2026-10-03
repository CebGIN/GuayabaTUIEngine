#include <guayaba/Core/Script.hpp>
#include <guayaba/Nodes/Node.hpp>

namespace guayaba {

void Node::enter_tree() {
    in_tree = true;
    if (script) script->on_enter_tree();
    for (const auto& child : children) {
        child->enter_tree();
    }
}

void Node::exit_tree() {
    for (const auto& child : children) {
        child->exit_tree();
    }
    if (script) script->on_exit_tree();
    in_tree = false;
}

Node::Node(const std::string& node_name) : name(node_name) {}

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
    if (script) script->on_process(delta_time);
    for (const auto& child : children) {
        child->update(delta_time);
    }
}

void Node::draw(ConsoleRenderer& renderer) {
    // The script draws here, ahead of the children, so that a child's on_draw
    // stays a true overlay instead of being buried under the parent's.
    if (script) script->on_draw(renderer);
    for (const auto& child : children) {
        child->draw(renderer);
    }
}

void Node::attach_script(std::shared_ptr<Script> new_script) {
    if (!new_script) return;

    // A bound script is never stolen from its host, and re-attaching to the very
    // same node is a no-op as well: bind_to() set owner on the first attach.
    std::shared_ptr<Node> bound_to = new_script->owner.lock();
    if (bound_to) return;

    if (script) {
        script->on_detached();
        script->unbind();
        script.reset();
    }

    script = std::move(new_script);
    script->bind_to(shared_from_this());
    script->on_attached();
}

void Node::detach_script() {
    if (!script) return;
    script->on_detached();
    script->unbind();
    script.reset();
}

void Node::invalidate_cache_recursively(){
    for (const auto& child : children) {
        child->invalidate_cache_recursively();
    }
}

} // namespace guayaba