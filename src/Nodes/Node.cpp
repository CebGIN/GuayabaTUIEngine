#include <algorithm>

#include <guayaba/Core/Script.hpp>
#include <guayaba/Nodes/Node.hpp>

#include "../Core/DeletionQueue.hpp"

namespace guayaba {

// Shared by update, draw and the tree propagation: see Node::for_each_child.
template<typename Fn>
void Node::for_each_child(Fn&& fn) {
    for (size_t i = 0; i < children.size(); ++i) {
        Node* child = children[i].get();
        fn(child);
        // The callback may have removed this child, which shifts the tail left
        // and would make us skip whatever moved into this slot. Comparing the
        // vector's own storage is safe even though child itself may already be
        // destroyed.
        if (i < children.size() && children[i].get() != child) --i;
    }
}

Node::Node(std::string node_name) : name(std::move(node_name)) {
    instance_id_ = detail::NodeRegistry::instance().register_node(this);
}

Node::~Node() {
    // The one place the registry is told. Every destruction path - the parent's
    // unique_ptr, the deletion queue, stack unwinding - arrives here, so there is
    // no way to free a node without the id being invalidated.
    if (instance_id_ != detail::NodeRegistry::INVALID_ID) {
        detail::NodeRegistry::instance().unregister_node(instance_id_);
        instance_id_ = detail::NodeRegistry::INVALID_ID;
    }
}

void Node::enter_tree() {
    in_tree = true;
    if (script) script->on_enter_tree();
    for_each_child([](Node* child) { child->enter_tree(); });
}

void Node::exit_tree() {
    // Children first, so a child's on_exit_tree sees the tree it is leaving.
    for_each_child([](Node* child) { child->exit_tree(); });
    if (script) script->on_exit_tree();
    in_tree = false;
}

std::string Node::make_child_name_unique(const std::string& wanted) const {
    auto taken = [this](const std::string& candidate) {
        for (const auto& child : children) {
            if (child->name == candidate) return true;
        }
        return false;
    };

    if (!taken(wanted)) return wanted;

    for (int suffix = 2;; ++suffix) {
        std::string candidate = wanted + "@" + std::to_string(suffix);
        if (!taken(candidate)) return candidate;
    }
}

Node* Node::add_child(std::unique_ptr<Node> child) {
    if (!child) return nullptr;

    if (child.get() == this) {
        throw std::logic_error("Node::add_child: '" + name + "' cannot be its own child.");
    }
    if (child->parent != nullptr) {
        throw std::logic_error("Node::add_child: '" + child->name
            + "' already has a parent '" + child->parent->name
            + "'. Use reparent() to move it.");
    }
    if (child->is_ancestor_of(this)) {
        throw std::logic_error("Node::add_child: '" + child->name + "' is an ancestor of '"
            + name + "', so this would create a cycle.");
    }

    child->name = make_child_name_unique(child->name);
    child->parent = this;

    Node* handle = child.get();
    children.push_back(std::move(child));

    if (in_tree) handle->enter_tree();
    return handle;
}

std::unique_ptr<Node> Node::detach_ownership() {
    std::unique_ptr<Node> owned;
    if (parent != nullptr) {
        owned = parent->remove_child(this);
    }
    return owned;
}

std::unique_ptr<Node> Node::remove_child(Node* child) {
    if (child == nullptr || child->parent != this) return nullptr;

    // Leave the tree before the subtree is torn down, so every descendant's
    // on_exit_tree fires while the tree is still coherent.
    if (child->in_tree) child->exit_tree();

    const auto it = std::find_if(children.begin(), children.end(),
        [child](const std::unique_ptr<Node>& held) { return held.get() == child; });
    if (it == children.end()) return nullptr;

    std::unique_ptr<Node> owned = std::move(*it);
    children.erase(it);

    child->parent = nullptr;
    child->invalidate_cache_recursively();
    return owned;
}

void Node::reparent(Node* new_parent) {
    if (new_parent == nullptr) {
        throw std::logic_error("Node::reparent: new parent cannot be null.");
    }
    if (parent == nullptr) {
        throw std::logic_error("Node::reparent: '" + name + "' has no parent to move from.");
    }
    if (new_parent == this) {
        throw std::logic_error("Node::reparent: '" + name + "' cannot be reparented to itself.");
    }
    if (new_parent == parent) return;

    if (new_parent->is_ancestor_of(this)) {
        throw std::logic_error("Node::reparent: '" + new_parent->name + "' is a descendant of '"
            + name + "', so this would create a cycle.");
    }

    // Take ownership back first: the unique_ptr in the old parent is what we are
    // moving, and add_child() requires an unparented node.
    std::unique_ptr<Node> owned = parent->remove_child(this);
    if (!owned) {
        throw std::logic_error("Node::reparent: '" + name + "' was detached mid-call.");
    }
    new_parent->add_child(std::move(owned));
}

void Node::free() {
    if (queued_for_deletion) return;

    if (parent == nullptr) {
        // Roots belong to whoever created them - the SceneManager for an active
        // scene, or the caller's own unique_ptr. Only an owner may destroy one.
        throw std::logic_error("Node::free: '" + name
            + "' has no parent, so the tree does not own it. Use change_scene(nullptr) or"
            " take_scene() to release a scene root.");
    }

    // Unparent now: leaves the tree and stops drawing this frame. Destroyed at the
    // next drain, which is what keeps this safe from inside a callback.
    queued_for_deletion = true;
    detail::enqueue_detached_deletion(detach_ownership());
}

void Node::queue_free() {
    if (queued_for_deletion) return;

    if (parent == nullptr) {
        throw std::logic_error("Node::queue_free: '" + name
            + "' has no parent, so the tree does not own it. Use change_scene(nullptr) or"
            " take_scene() to release a scene root.");
    }

    // Stays parented, so it keeps drawing this frame; the drain destroys it.
    queued_for_deletion = true;
    detail::schedule_deletion(this);
}

bool Node::has_child(const std::string& child_name) const {
    return find_child(child_name, false) != nullptr;
}

int Node::get_index(Node* child) const {
    for (size_t i = 0; i < children.size(); ++i) {
        if (children[i].get() == child) return static_cast<int>(i);
    }
    return -1;
}

void Node::move_child(Node* child, size_t to_index) {
    if (child == nullptr || child->parent != this) {
        throw std::logic_error("Node::move_child: node is not a child of '" + name + "'.");
    }
    if (to_index >= children.size()) {
        throw std::out_of_range("Node::move_child: Index out of bounds. en: " + name);
    }

    const auto it = std::find_if(children.begin(), children.end(),
        [child](const std::unique_ptr<Node>& held) { return held.get() == child; });
    if (it == children.end()) return;

    std::unique_ptr<Node> owned = std::move(*it);
    children.erase(it);
    children.insert(children.begin() + static_cast<std::ptrdiff_t>(to_index), std::move(owned));
}

Node* Node::find_child(const std::string& wanted_name, bool recursive) const {
    for (const auto& child : children) {
        if (child->name == wanted_name) return child.get();
        if (recursive) {
            if (Node* found = child->find_child(wanted_name, true)) return found;
        }
    }
    return nullptr;
}

Node* Node::resolve_path(const std::string& path) const {
    if (path.empty()) return nullptr;

    const bool absolute = path.front() == '/';

    // Resolution walks the tree but never mutates it; same const_cast Godot uses
    // in get_node_or_null().
    Node* const self = const_cast<Node*>(this);

    // An absolute path is resolved from the topmost ancestor, and its FIRST
    // segment names that ancestor itself - so get_node(get_path()) round-trips.
    // A relative path starts at this node and its first segment is a child.
    Node* current = absolute ? nullptr : self;
    Node* root = self;
    if (absolute) {
        while (root->parent != nullptr) root = root->parent;
    }

    size_t pos = 0;
    while (pos <= path.size()) {
        const size_t slash = path.find('/', pos);
        const std::string segment =
            path.substr(pos, slash == std::string::npos ? std::string::npos : slash - pos);

        if (!segment.empty()) {
            Node* next = nullptr;
            if (segment == ".") {
                next = current;
            } else if (segment == "..") {
                if (current == nullptr || current->parent == nullptr) return nullptr;
                next = current->parent;
            } else if (current == nullptr) {
                // Reached only by the leading segment of an absolute path.
                if (segment == root->name) next = root;
            } else {
                next = current->find_child(segment, false);
            }

            if (next == nullptr) return nullptr;
            current = next;
        }

        if (slash == std::string::npos) break;
        pos = slash + 1;
    }

    return current;
}

std::string Node::get_path() const {
    if (parent == nullptr) return "/" + name;

    std::vector<std::string> segments;
    for (const Node* n = this; n != nullptr; n = n->parent) {
        segments.push_back(n->name);
    }

    std::string path;
    for (auto it = segments.rbegin(); it != segments.rend(); ++it) {
        path += "/";
        path += *it;
    }
    return path;
}

std::string Node::get_path_to(const Node* other) const {
    if (other == nullptr) return "";

    // Both chains root-first, so the shared prefix is the common ancestor.
    std::vector<const Node*> from_chain;
    std::vector<const Node*> to_chain;
    for (const Node* n = this; n != nullptr; n = n->parent) from_chain.push_back(n);
    for (const Node* n = other; n != nullptr; n = n->parent) to_chain.push_back(n);
    std::reverse(from_chain.begin(), from_chain.end());
    std::reverse(to_chain.begin(), to_chain.end());

    size_t common = 0;
    while (common < from_chain.size() && common < to_chain.size()
        && from_chain[common] == to_chain[common]) {
        ++common;
    }

    std::string path;
    for (size_t i = common; i < from_chain.size(); ++i) path += "/..";
    for (size_t i = common; i < to_chain.size(); ++i) {
        path += "/";
        path += to_chain[i]->get_name();
    }
    return path.empty() ? "." : path;
}

Node* Node::resolve_instance_id(uint64_t id) {
    return detail::NodeRegistry::instance().resolve(id);
}

size_t Node::live_node_count() {
    return detail::NodeRegistry::instance().live_count();
}

NodeType Node::get_type() const {
    return NodeType::NODE;
}

void Node::update(double delta_time) {
    if (script) script->on_process(delta_time);
    for_each_child([delta_time](Node* child) { child->update(delta_time); });
}

void Node::draw(ConsoleRenderer& renderer) {
    // The script draws here, ahead of the children, so that a child's on_draw
    // stays a true overlay instead of being buried under the parent's.
    if (script) script->on_draw(renderer);
    for_each_child([&renderer](Node* child) { child->draw(renderer); });
}

void Node::attach_script(std::unique_ptr<Script> new_script) {
    if (!new_script) return;

    // A bound script is never stolen from its host.
    if (new_script->is_bound()) return;

    detach_script();

    script = std::move(new_script);
    script->bind_to(this);
    script->on_attached();
}

void Node::detach_script() {
    if (!script) return;
    script->on_detached();
    script->unbind();
    script.reset();
}

void Node::invalidate_cache_recursively() {
    for_each_child([](Node* child) { child->invalidate_cache_recursively(); });
}

} // namespace guayaba