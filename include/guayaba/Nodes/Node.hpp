#ifndef GUAYABA_NODES_NODE_HPP
#define GUAYABA_NODES_NODE_HPP

#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include <guayaba/Core/NodeType.hpp>

namespace guayaba {

class SceneManager;
class ConsoleRenderer;
class Script;

class Node : public std::enable_shared_from_this<Node>{
    friend class SceneManager;
protected:
    std::vector<std::shared_ptr<Node>> children;
    std::weak_ptr<Node> parent;
    std::string name;

    // Optional behaviour attached from outside.
    std::shared_ptr<Script> script;

    mutable bool in_tree = false;

    virtual void enter_tree();
    virtual void exit_tree();

public:
    Node(const std::string& node_name = "Unnamed Node");

    virtual ~Node();

    void set_parent(std::shared_ptr<Node> parent_node);

    std::shared_ptr<Node> get_parent() const;

    template<typename T>
    std::shared_ptr<T> get_parent_of_type() const {
        static_assert(std::is_base_of<Node, T>::value, "T must be a Node type.");
        std::shared_ptr<Node> current_parent = get_parent();
        while (current_parent) {
            std::shared_ptr<T> parent_of_type = std::dynamic_pointer_cast<T>(current_parent);
            if (parent_of_type) {
                return parent_of_type;
            }
            current_parent = current_parent->get_parent();
        }
        return nullptr;
    }

    bool is_root() const;

    void add_child(std::shared_ptr<Node> child_node);

    size_t get_child_count() const;

    std::shared_ptr<Node> get_child(size_t index);

    std::shared_ptr<const Node> get_child(size_t index) const;

    std::vector<std::shared_ptr<Node>>::iterator begin() { return children.begin();}
    std::vector<std::shared_ptr<Node>>::iterator end() { return children.end();}

    std::vector<std::shared_ptr<Node>>::const_iterator begin() const { return children.cbegin();}
    std::vector<std::shared_ptr<Node>>::const_iterator end() const { return children.cend();}

    const std::string& get_name() const;

    virtual NodeType get_type() const;

    // An override that fails to delegate to the base implementation silently
    // disables the attached script for that node type. Every override of update
    // and draw MUST end with Node::update(...) / Node::draw(...).
    virtual void update(double delta_time);

    bool is_in_tree() const { return in_tree; }

    // At most one script per node. Attaching a null script, or one that is
    // already bound to any node, is a silent no-op. Replacing this node's own
    // script fires on_detached on the old one, never on_exit_tree.
    // Requires the node to be owned by a shared_ptr.
    void attach_script(std::shared_ptr<Script> new_script);

    void detach_script();

    bool has_script() const { return static_cast<bool>(script); }

    std::shared_ptr<Script> get_script() const { return script; }

    // Rarely needed: you usually kept your own reference when attaching. This is
    // for when you hold nothing but the node. Returns nullptr on a type mismatch.
    template<typename T>
    std::shared_ptr<T> get_script_as() const {
        static_assert(std::is_base_of<Script, T>::value, "T must be a Script type.");
        return std::dynamic_pointer_cast<T>(script);
    }

    virtual void draw(ConsoleRenderer& renderer);

    virtual void invalidate_cache_recursively();
};

} // namespace guayaba

#endif // GUAYABA_NODES_NODE_HPP