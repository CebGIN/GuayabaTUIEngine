#ifndef GUAYABA_NODES_NODE_HPP
#define GUAYABA_NODES_NODE_HPP

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include <guayaba/Core/NodeType.hpp>

namespace guayaba {

class SceneManager;
class ConsoleRenderer;

class Node : public std::enable_shared_from_this<Node>{
    friend class SceneManager;
protected:
    std::vector<std::shared_ptr<Node>> children;
    std::weak_ptr<Node> parent;
    std::string name;
    std::function<void(double)> process;
    std::function<void()> at_enter_tree;
    std::function<void()> at_exit_tree;

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

    virtual void update(double delta_time);

    void set_process_function(std::function<void(double)> func);

    void set_at_enter_function(std::function<void()> func);

    void set_at_exit_function(std::function<void()> func);

    virtual void draw(ConsoleRenderer& renderer);

    virtual void invalidate_cache_recursively();
};

} // namespace guayaba

#endif // GUAYABA_NODES_NODE_HPP