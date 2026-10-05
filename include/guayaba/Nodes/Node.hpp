#ifndef GUAYABA_NODES_NODE_HPP
#define GUAYABA_NODES_NODE_HPP

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <guayaba/Core/NodeRegistry.hpp>
#include <guayaba/Core/NodeType.hpp>

namespace guayaba {

class SceneManager;
class ConsoleRenderer;
class Script;

/*
    OWNERSHIP
    The tree owns its children outright (unique_ptr) and holds its parent as a
    plain pointer. There is no refcounting anywhere in the tree, which means a
    reference cycle is not merely discouraged, it cannot be written.

    The rule that follows: a node belongs to whoever created it right up to the
    moment it is handed over, and to the tree from then on.

        auto root = std::make_unique<Node>("root");   // you own it
        NodeUI* title = root->add_child<NodeUI>(...); // tree owns it now
        SceneManager::get_instance().change_scene(std::move(root));

    Once handed over you hold only a raw T*, which is safe as long as the node is
    inside the subtree that contains you - ancestors and same-subtree nodes
    cannot be freed ahead of you. For references that cross that boundary use
    NodeRef<T>: see NodeRef.hpp for why raw is not enough there.

    Non-copyable: a copy would duplicate the instance id, leaving one slot
    pointing at two nodes, and the first destructor to run would invalidate the
    survivor's handles.
*/
class Node {
    friend class SceneManager;

protected:
    std::vector<std::unique_ptr<Node>> children;
    Node* parent = nullptr;
    std::string name;
    std::unique_ptr<Script> script;

    mutable bool in_tree = false;
    bool queued_for_deletion = false;

    virtual void enter_tree();
    virtual void exit_tree();

    // Iterates children in a way that tolerates the tree being mutated from
    // inside a callback: adding is fine, and a child that removes itself is
    // re-stepped rather than skipped. Reads only the vector's storage, never the
    // child, which may already be destroyed.
    template<typename Fn>
    void for_each_child(Fn&& fn);

private:
    uint64_t instance_id_ = detail::NodeRegistry::INVALID_ID;

    // Hands this node's ownership to the caller: unparents, leaves the tree,
    // fires on_exit_tree. Used by remove_child and by the deletion path.
    std::unique_ptr<Node> detach_ownership();

    std::string make_child_name_unique(const std::string& wanted) const;

    Node* resolve_path(const std::string& path) const;

public:
    explicit Node(std::string node_name = "Unnamed Node");

    virtual ~Node();

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    Node(Node&&) = delete;
    Node& operator=(Node&&) = delete;

    // --- Building -----------------------------------------------------------

    // Constructs in place and hands the ownership to this node. A duplicate
    // sibling name becomes "name@2", so paths stay unambiguous.
    template<typename T, typename... Args>
    T* add_child(Args&&... args) {
        static_assert(std::is_base_of<Node, T>::value, "T must be a Node type.");
        std::unique_ptr<Node> owned(new T(std::forward<Args>(args)...));
        T* handle = static_cast<T*>(owned.get());
        add_child(std::move(owned));
        return handle;
    }

    // The ownership hand-off. A node that already has a parent is rejected: use
    // reparent(), which leaves the old tree properly first.
    Node* add_child(std::unique_ptr<Node> child);

    // Detaches and hands ownership back. Never destroys, so unlike Godot's
    // remove_child() it cannot produce an orphan you forgot to free - if you
    // drop the returned unique_ptr, the subtree goes with it.
    std::unique_ptr<Node> remove_child(Node* child);

    // remove_child + add_child, with enter_tree/exit_tree handled in between.
    void reparent(Node* new_parent);

    // --- Destruction --------------------------------------------------------

    // REQUIRES THE FRAME LOOP. Both of these only arrange destruction; the
    // SceneManager performs it, at the end of the frame. Calling them outside a
    // running loop leaves the node pending and undestroyed.

    // Unparents now, so the node leaves the tree and stops drawing immediately.
    // Destroyed at the next drain, which is what makes this safe to call from
    // inside a callback.
    void free();

    // Stays parented: keeps updating and drawing until the drain, matching Godot,
    // where a queued node still renders its final frame.
    void queue_free();

    bool is_queued_for_deletion() const { return queued_for_deletion; }

    // --- Tree navigation ----------------------------------------------------

    Node* get_parent() const { return parent; }

    template<typename T>
    T* get_parent_of_type() const {
        static_assert(std::is_base_of<Node, T>::value, "T must be a Node type.");
        for (Node* p = parent; p != nullptr; p = p->parent) {
            if (T* typed = dynamic_cast<T*>(p)) return typed;
        }
        return nullptr;
    }

    bool is_root() const { return parent == nullptr; }

    // True when this node sits somewhere above the argument. Used to reject
    // add_child()/reparent() calls that would close a loop.
    bool is_ancestor_of(const Node* other) const {
        for (const Node* n = other; n != nullptr; n = n->parent) {
            if (n == this) return true;
        }
        return false;
    }

    size_t get_child_count() const { return children.size(); }

    template<typename T = Node>
    T* get_child(size_t index) const {
        static_assert(std::is_base_of<Node, T>::value, "T must be a Node type.");
        if (index >= children.size()) {
            throw std::out_of_range("Node::get_child(index): Index out of bounds. en: " + name);
        }
        return static_cast<T*>(children[index].get());
    }

    bool has_child(const std::string& child_name) const;

    // Draw order follows child order, so this is z-order.
    int get_index(Node* child) const;
    void move_child(Node* child, size_t to_index);

    // "a/b", "/a/b", "." and ".." are all understood, resolved against the tree
    // rather than stored, so a NodeRef plus a path survives reparenting.
    template<typename T = Node>
    T* get_node(const std::string& path) const {
        T* found = get_node_or_null<T>(path);
        if (!found) {
            throw std::runtime_error("Node::get_node(path): No node at \"" + path + "\" from \"" + get_path() + "\".");
        }
        return found;
    }

    template<typename T = Node>
    T* get_node_or_null(const std::string& path) const {
        static_assert(std::is_base_of<Node, T>::value, "T must be a Node type.");
        return static_cast<T*>(resolve_path(path));
    }

    // Pre-order, first match wins.
    Node* find_child(const std::string& wanted_name, bool recursive = true) const;

    std::string get_path() const;
    std::string get_path_to(const Node* other) const;

    // --- Identity -----------------------------------------------------------

    const std::string& get_name() const { return name; }

    uint64_t instance_id() const { return instance_id_; }

    // Resolves an id captured earlier. Returns nullptr once that node is gone.
    static Node* resolve_instance_id(uint64_t id);

    // Live nodes constructed but not yet destroyed. Non-zero after tearing the
    // scene down means something leaked.
    static size_t live_node_count();

    virtual NodeType get_type() const;

    bool is_in_tree() const { return in_tree; }

    // --- Behaviour ----------------------------------------------------------

    // An override that fails to delegate to the base implementation silently
    // disables the attached script for that node type. Every override of update
    // and draw MUST end with Node::update(...) / Node::draw(...).
    virtual void update(double delta_time);

    // At most one script per node, owned by it. A node outliving its script is
    // what makes Script::get_owner() safe without any check: while your method
    // runs, this node is guaranteed alive.
    template<typename T, typename... Args>
    T* add_script(Args&&... args) {
        static_assert(std::is_base_of<Script, T>::value, "T must be a Script type.");
        std::unique_ptr<Script> owned(new T(std::forward<Args>(args)...));
        T* handle = static_cast<T*>(owned.get());
        attach_script(std::move(owned));
        return handle;
    }

    // Attaching nullptr, or a script already bound to any node, is a silent
    // no-op. Replacing this node's own script fires on_detached on the old one,
    // never on_exit_tree.
    void attach_script(std::unique_ptr<Script> new_script);

    void detach_script();

    bool has_script() const { return static_cast<bool>(script); }

    // You normally kept the handle add_script<T>() returned. This is for when
    // you hold nothing but the node.
    template<typename T>
    T* get_script_as() const {
        static_assert(std::is_base_of<Script, T>::value, "T must be a Script type.");
        return dynamic_cast<T*>(script.get());
    }

    virtual void draw(ConsoleRenderer& renderer);

    virtual void invalidate_cache_recursively();
};

} // namespace guayaba

#endif // GUAYABA_NODES_NODE_HPP