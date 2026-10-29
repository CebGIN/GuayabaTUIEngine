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
        // The callback may have removed this child, shifting the tail left and
        // making us skip whatever moved into this slot. Comparing the vector's
        // own storage is safe even though child may already be destroyed.
        if (i < children.size() && children[i].get() != child) --i;
    }
}

Node::Node(std::string node_name) : name(std::move(node_name)) {
    instance_id_ = detail::NodeRegistry::instance().register_node(this);
}

Node::~Node() {
    // Runs before any member is destroyed, so `children` and `parent` are still
    // intact. Do not free the node from here.
    if (script) script->on_detached();

    // The one place the registry is told, so no destruction path can leave the id
    // resolving.
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
    // A '/' in a name would split into two path segments.
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

NodeRef<Node> Node::add_child(std::unique_ptr<Node>&& child) {
    if (!child) return NodeRef<Node>();

    if (child.get() == this) {
        throw std::logic_error("Node::add_child: '" + name + "' cannot be its own child.");
    }

    // Ancestry is checked by pointer, not by path: unnamed nodes all share the
    // path "/Unnamed Node", so a path comparison cannot tell two of them apart.
    for (const Node* up = this; up != nullptr; up = up->parent) {
        if (up == child.get()) {
            throw std::logic_error("Node::add_child: '" + child->name + "' is an ancestor of '"
                + name + "', so this would create a cycle.");
        }
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

    // Everything after the hole moved down one.
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
        // A root is owned by its creator or the SceneManager, neither reachable
        // from here.
        throw std::logic_error("Node::steal: '" + name + "' has no parent, so there is no"
            " tree to take it from. Use change_scene(nullptr) or take_scene() to release"
            " a scene root.");
    }
    return parent->remove_child(child_index_);
}

void Node::free() {
    if (queued_for_deletion) return;

    if (parent == nullptr) {
        // A root belongs to its creator or the SceneManager. Only an owner may
        // destroy one.
        throw std::logic_error("Node::free: '" + name
            + "' has no parent, so the tree does not own it. Use change_scene(nullptr) or"
            " take_scene() to release a scene root.");
    }

    // Unparents now, so it leaves the tree immediately; destroyed at the next
    // drain.
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

    // `to` is the position the node ends up at; after the erase the indices
    // already line up, so `to` needs no adjustment. to == from is a no-op.
    std::unique_ptr<Node> owned = std::move(children[from]);
    children.erase(children.begin() + static_cast<std::ptrdiff_t>(from));
    children.insert(children.begin() + static_cast<std::ptrdiff_t>(to), std::move(owned));

    // Both halves moved things, so the whole vector is stale.
    reindex_from(0);
}

NodeRef<Node> Node::get_parent() const {
    // The NodeRef(N*) constructor dereferences its argument, so a null gets the
    // empty ref.
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

    // Resolution walks the tree but never mutates it.
    Node* const self = const_cast<Node*>(this);

    // An absolute path is resolved from the topmost ancestor, and its FIRST
    // segment names that ancestor itself, so get_node(get_path()) round-trips.
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
    // Names cannot contain '/', so a longer path prefixed by mine names a
    // descendant. A node is also its own ancestor.
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
    // strings lets this answer for a freed node.
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

void Node::update(double delta_time) {
    const uint64_t my_id = instance_id_;

    if (script) script->on_process(delta_time);
    if (!detail::NodeRegistry::instance().is_live(my_id)) return;

    for_each_child([delta_time](Node* child) { child->update(delta_time); });
}

void Node::update_layout(const Rect& parent_rect) {
    // Hands the rect straight to every child. A Control resolves itself first and
    // calls back in with its own rect.
    for_each_child([&parent_rect](Node* child) { child->update_layout(parent_rect); });
}

void Node::route_input(std::vector<InputEvent>& events) {
    // Same guard as update(): a child may free itself while the walk holds a
    // pointer to it.
    const uint64_t my_id = instance_id_;
    if (!detail::NodeRegistry::instance().is_live(my_id)) return;

    // Children in reverse, so a later sibling (drawn on top) gets first refusal.
    // The count is taken once; a freed child leaves a slot the bounds check skips.
    for (size_t i = children.size(); i > 0; --i) {
        if (i - 1 >= children.size()) continue;
        children[i - 1]->route_input(events);

        if (!detail::NodeRegistry::instance().is_live(my_id)) return;
        if (events.empty()) return;
    }

    // This node's turn, for whatever the children left. Only a script can act here.
    if (script) {
        for (size_t i = 0; i < events.size();) {
            if (script->on_input(events[i])) {
                events.erase(events.begin() + static_cast<std::ptrdiff_t>(i));
                // Do not advance: the next event shifted into this slot.
            } else {
                ++i;
            }
        }
    }
}

void Node::route_unhandled_input(const InputEvent& event) {
    if (script) script->on_unhandled_input(event);

    const uint64_t my_id = instance_id_;
    if (!detail::NodeRegistry::instance().is_live(my_id)) return;

    for_each_child([&event](Node* child) { child->route_unhandled_input(event); });
}

void Node::draw(ConsoleRenderer& renderer, const Rect& clip) {
    // Only the children: a script has no drawing hook, so no user code runs that
    // could free this node mid-frame.
    for_each_child([&renderer, &clip](Node* child) { child->draw(renderer, clip); });
}

void Node::attach_script(std::unique_ptr<ScriptBase> new_script) {
    if (!new_script) return;

    // Sealed on the first successful attach and never reopened, so a script lives
    // as long as its node. ScriptRef::script() depends on this.
    if (script_sealed) {
        throw std::logic_error("Node::attach_script: '" + name + "' already has a script, and a script"
            " is kept for as long as its node. Detaching does not exist, so build the subtree"
            " before handing the node to a scene.");
    }

    // Throws if the script is not for this node type, before anything is stored or
    // sealed.
    new_script->bind_to(this);
    script = std::move(new_script);
    script_sealed = true;

    // Unconditional: binding only happens while building.
    script->on_attached();
}

void Node::invalidate_cache_recursively() {
    for_each_child([](Node* child) { child->invalidate_cache_recursively(); });
}

} // namespace guayaba