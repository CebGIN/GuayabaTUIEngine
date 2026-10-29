#ifndef GUAYABA_CORE_NODEREF_HPP
#define GUAYABA_CORE_NODEREF_HPP

#include <cassert>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include <guayaba/Core/NodeRegistry.hpp>
#include <guayaba/Nodes/Node.hpp>

namespace guayaba {

/*
    Non-owning, dangling-safe handle to a Node. Stores a packed id rather than a
    pointer, so it cannot dangle or alias a different node built into the same
    address.

    A raw N* is the right handle inside the subtree you built. Across subtrees it
    is not: the caller may free that subtree at any time. Store a NodeRef there and
    test it.

    A NodeRef names one node for its whole life, so the target has to arrive with
    it - in practice through the constructor:

        class Follower : public Script<Node2D> {
        public:
            explicit Follower(Node2D* target_node) : target(target_node) {}
            void on_process(double) override {
                if (auto t = target.get()) t->set_local_position(Input::mouse_pos);
            }
            NodeRef<Node2D> target;   // lives in someone else's subtree
        };

    get() returns nullptr once the node has been destroyed, and never a pointer to
    a different node. It does not keep the node alive and says nothing about
    whether the node is still in the tree. Use get_node_or_null() when you want to
    resolve by relationship rather than identity.
*/
template<typename N>
class NodeRef {
    static_assert(std::is_base_of<Node, N>::value, "N must be a Node type.");

public:
    NodeRef() = default;

    // Requires a valid N*: the pointer is dereferenced to take its id.
    explicit NodeRef(N* raw) : id_(raw->instance_id()) {}

    // Widening only: NodeRef<Derived> becomes NodeRef<Base>. Narrowing is left to
    // get() and get_node<T>().
    template<typename D, typename = std::enable_if_t<std::is_base_of<N, D>::value>>
    NodeRef(const NodeRef<D>& derived) : id_(derived.id()) {}

    // The static_cast is sound: the id was captured from an N*, a resolving id
    // names that same object (generations never repeat), and an object's dynamic
    // type cannot change.
    N* get() const {
        Node* node = detail::NodeRegistry::instance().resolve(id_);
        return node ? static_cast<N*>(node) : nullptr;
    }

    bool is_valid() const { return get() != nullptr; }

    explicit operator bool() const { return is_valid(); }

    N* operator->() const {
        N* node = get();
        assert(node != nullptr && "NodeRef::operator-> on a dead reference.");
        return node;
    }

    uint64_t id() const { return id_; }

private:
    uint64_t id_ = detail::NodeRegistry::INVALID_ID;
};

/*
    Node members that return a NodeRef are declared in Nodes/Node.hpp and defined
    here, because returning one by value needs the complete type and Node.hpp
    cannot include this header.
*/
template<typename T, typename... Args>
NodeRef<T> Node::add_child(Args&&... args) {
    static_assert(std::is_base_of<Node, T>::value, "T must be a Node type.");
    std::unique_ptr<Node> owned(new T(std::forward<Args>(args)...));
    T* handle = static_cast<T*>(owned.get());
    add_child(std::move(owned));
    return NodeRef<T>(handle);
}

template<typename T>
NodeRef<T> Node::get_parent_of_type() const {
    static_assert(std::is_base_of<Node, T>::value, "T must be a Node type.");
    for (Node* p = parent; p != nullptr; p = p->parent) {
        if (T* typed = dynamic_cast<T*>(p)) return NodeRef<T>(typed);
    }
    return NodeRef<T>();
}

template<typename T>
NodeRef<T> Node::get_child(size_t index) const {
    static_assert(std::is_base_of<Node, T>::value, "T must be a Node type.");
    if (index >= children.size()) {
        throw std::out_of_range("Node::get_child(index): Index out of bounds. en: " + name);
    }
    return NodeRef<T>(static_cast<T*>(children[index].get()));
}

template<typename T>
NodeRef<T> Node::get_node(const std::string& path) const {
    const NodeRef<T> found = get_node_or_null<T>(path);
    if (!found) {
        throw std::runtime_error("Node::get_node(path): No node at \"" + path
            + "\" from \"" + get_path() + "\".");
    }
    return found;
}

template<typename T>
NodeRef<T> Node::get_node_or_null(const std::string& path) const {
    static_assert(std::is_base_of<Node, T>::value, "T must be a Node type.");
    // A miss is an empty ref; the NodeRef(N*) constructor cannot take nullptr.
    T* found = static_cast<T*>(resolve_path(path));
    return found != nullptr ? NodeRef<T>(found) : NodeRef<T>();
}

} // namespace guayaba

#endif // GUAYABA_CORE_NODEREF_HPP