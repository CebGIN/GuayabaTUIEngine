#include <algorithm>

#include <guayaba/Core/NodeRef.hpp>
#include <guayaba/Core/Script.hpp>
#include <guayaba/Core/ScriptRef.hpp>
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
    // Last chance to tell the script it is going away. This runs before any
    // member is destroyed, so `children` and `parent` are still intact and a
    // script's `node` reads as a coherent node - do not free it from here.
    if (script) script->on_detached();

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
    // Paths are compared as strings now (is_ancestor_of, get_path_to), so a '/'
    // inside a name would split into two segments and make two distinct nodes
    // look like the same path.
    if (wanted.find('/') != std::string::npos) {
        throw std::logic_error("Node: name '" + wanted + "' cannot contain '/'.");
    }

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

NodeRef<Node> Node::add_child(std::unique_ptr<Node> child) {
    if (!child) return NodeRef<Node>();

    if (child.get() == this) {
        throw std::logic_error("Node::add_child: '" + name + "' cannot be its own child.");
    }

    if (child->is_ancestor_of(get_path())) {
        throw std::logic_error("Node::add_child: '" + child->name + "' is an ancestor of '"
            + name + "', so this would create a cycle.");
    }

    child->name = make_child_name_unique(child->name);
    child->parent = this;

    // Only this one is stamped: push_back appends, so nothing else moves.
    child->child_index_ = children.size();

    Node* handle = child.get();
    children.push_back(std::move(child));

    if (in_tree) handle->enter_tree();
    return NodeRef<Node>(handle);
}

std::unique_ptr<Node> Node::detach_ownership() {
    std::unique_ptr<Node> owned;
    if (parent != nullptr) {
        owned = parent->remove_child(child_index_);
    }
    return owned;
}

std::unique_ptr<Node> Node::remove_child(size_t index) {
    if (index >= children.size()) {
        throw std::out_of_range("Node::remove_child: Index out of bounds. en: " + name);
    }

    Node* child = children[index].get();

    // Leave the tree before the subtree is torn down, so every descendant's
    // on_exit_tree fires while the tree is still coherent.
    if (child->in_tree) child->exit_tree();

    std::unique_ptr<Node> owned = std::move(children[index]);
    children.erase(children.begin() + static_cast<std::ptrdiff_t>(index));

    // Everything after the hole moved down one; the detached child no longer has
    // a slot here at all.
    reindex_from(index);

    child->parent = nullptr;
    child->child_index_ = SIZE_MAX;
    child->invalidate_cache_recursively();
    return owned;
}

void Node::reindex_from(size_t start) {
    for (size_t i = start; i < children.size(); ++i) {
        children[i]->child_index_ = i;
    }
}

std::unique_ptr<Node> Node::steal() {
    if (parent == nullptr) {
        // A root is owned by whoever created it, or by the SceneManager for an
        // active scene. Neither is reachable from here, so claiming it here
        // would be claiming a node whose owner we cannot see.
        throw std::logic_error("Node::steal: '" + name + "' has no parent, so there is no"
            " tree to take it from. Use change_scene(nullptr) or take_scene() to release"
            " a scene root.");
    }
    return parent->remove_child(child_index_);
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
    return static_cast<bool>(find_child(child_name, false));
}

void Node::move_child(size_t from, size_t to) {
    if (from >= children.size() || to >= children.size()) {
        throw std::out_of_range("Node::move_child: Index out of bounds. en: " + name);
    }

    // `to` is the position the node ends up at. After the erase the vector is one
    // shorter, and its indices already line up with the final positions - the
    // nodes between `from` and `to` have each shifted down one, which is what
    // leaves room for the insert. So `to` needs no adjustment either way, and
    // to == from is a no-op rather than a self-insert.
    std::unique_ptr<Node> owned = std::move(children[from]);
    children.erase(children.begin() + static_cast<std::ptrdiff_t>(from));
    children.insert(children.begin() + static_cast<std::ptrdiff_t>(to), std::move(owned));

    // Both halves of this reshuffle moved things, so the whole vector is stale.
    reindex_from(0);
}

NodeRef<Node> Node::get_parent() const {
    // A root has no parent, and the NodeRef(N*) constructor dereferences to read
    // the id, so it cannot be handed a null.
    return parent != nullptr ? NodeRef<Node>(parent) : NodeRef<Node>();
}

NodeRef<Node> Node::find_child(const std::string& wanted_name, bool recursive) const {
    for (const auto& child : children) {
        if (child->name == wanted_name) return NodeRef<Node>(child.get());
        if (recursive) {
            const NodeRef<Node> found = child->find_child(wanted_name, true);
            if (found) return found;
        }
    }
    return NodeRef<Node>();
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
                next = current->find_child(segment, false).get();
            }

            if (next == nullptr) return nullptr;
            current = next;
        }

        if (slash == std::string::npos) break;
        pos = slash + 1;
    }

    return current;
}

bool Node::is_ancestor_of(const std::string& path) const {
    // Names cannot contain '/', so a path is strictly longer than this node's
    // and still prefixed by it only when it names a descendant. Equal counts too:
    // a node is its own ancestor, which is what the old pointer walk reported.
    const std::string mine = get_path();
    if (path == mine) return true;
    if (path.size() <= mine.size()) return false;
    return path.compare(0, mine.size(), mine) == 0 && path[mine.size()] == '/';
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

std::string Node::get_path_to(const std::string& target) const {
    const auto segments = [](const std::string& p) {
        std::vector<std::string> out;
        size_t pos = 1;                        // skip the leading '/'
        while (pos <= p.size()) {
            const size_t slash = p.find('/', pos);
            const std::string seg =
                p.substr(pos, slash == std::string::npos ? std::string::npos : slash - pos);
            if (!seg.empty()) out.push_back(seg);
            if (slash == std::string::npos) break;
            pos = slash + 1;
        }
        return out;
    };

    // Both root-first, so the shared prefix is the common ancestor. Working on
    // strings rather than nodes is what lets this answer for a freed node.
    const std::vector<std::string> from_chain = segments(get_path());
    const std::vector<std::string> to_chain = segments(target);

    size_t common = 0;
    while (common < from_chain.size() && common < to_chain.size()
        && from_chain[common] == to_chain[common]) {
        ++common;
    }

    std::string result;
    for (size_t i = common; i < from_chain.size(); ++i) result += "/..";
    for (size_t i = common; i < to_chain.size(); ++i) {
        result += "/";
        result += to_chain[i];
    }
    return result.empty() ? "." : result;
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

void Node::attach_script(std::unique_ptr<ScriptBase> new_script) {
    if (!new_script) return;

    // Sealed on the first successful attach and never reopened, so a script
    // lives exactly as long as its node. This is the rule ScriptRef's cast-free
    // script() depends on: see Core/ScriptRef.hpp.
    if (script_sealed) {
        throw std::logic_error("Node::attach_script: '" + name + "' already has a script, and a script"
            " is kept for as long as its node. Detaching does not exist, so build the subtree"
            " before handing the node to a scene.");
    }

    // Throws if the Script isn't for this NodeType. Checked before anything is
    // stored or sealed, so a rejected attach leaves the node exactly as it was.
    new_script->bind_to(this);
    script = std::move(new_script);
    script_sealed = true;

    // Unconditional now: binding only happens while building, which is exactly
    // when this is worth saying. It used to be gated on is_in_tree(), which made
    // it fire only for the runtime attach that no longer exists.
    script->on_attached();
}

void Node::invalidate_cache_recursively() {
    for_each_child([](Node* child) { child->invalidate_cache_recursively(); });
}

} // namespace guayaba