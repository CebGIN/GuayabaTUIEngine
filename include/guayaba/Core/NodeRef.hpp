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
    Non-owning, dangling-safe handle to a Node. Eight bytes: a packed id, no
    pointer, so it cannot dangle and cannot silently alias a node that was
    constructed into the same address afterwards.

    WHEN YOU NEED THIS
    Raw N* is the right handle for anything inside the subtree you built: an
    ancestor of the holder, or anything the holder's subtree owns. That
    containment is structural, because the tree owns its children outright.

    A reference that crosses that boundary cannot be structural. The clearest
    case is a node handed to a builder as an argument: the caller may free that
    subtree at any time, and nothing in the receiving subtree keeps it alive.
    Store a NodeRef there and test it.

    A NodeRef names one node for its whole life. It is built from a pointer and
    it never changes what it points at, so a reference has to arrive with its
    target - which in practice means through the constructor:

        class Follower : public Script<Node2D> {
        public:
            explicit Follower(Node2D* target_node) : target(target_node) {}
            void on_process(double) override {
                if (auto t = target.get()) t->set_local_position(Input::mouse_pos);
            }
            NodeRef<Node2D> target;   // lives in someone else's subtree
        };

        body->attach_script<Follower>(body_in_another_subtree);

    attach_script forwards its arguments to S, so the target reaches the script
    the same way any other constructor argument would.

    GUARANTEES
    get() returns nullptr once the node has been destroyed, from the moment its
    destructor runs. It never returns a pointer to a different node. It does not
    keep the node alive, and it says nothing about whether the node is still in
    the scene tree - a live handle to an orphan is still live.

    WHAT IT DOES NOT DO
    It does not detect a reference that is valid but stale, e.g. a node that was
    reparented somewhere else. Use get_node_or_null() when you want to resolve by
    relationship rather than identity.
*/
template<typename N>
class NodeRef {
    static_assert(std::is_base_of<Node, N>::value, "N must be a Node type.");

public:
    NodeRef() = default;

    // REQUIRES A VALID N*. The pointer is dereferenced to take its id, so a null
    // or already-destroyed argument is undefined behaviour rather than an empty
    // handle: this class trusts its caller to know what it is pointing at.
    explicit NodeRef(N* raw) : id_(raw->instance_id()) {}

    // Widening only: NodeRef<Derived> becomes NodeRef<Base>. The id is the same
    // object either way, and Base is what NodeRef<Base>::get() can do soundly
    // with it. Narrowing is left out on purpose - that is what get() and
    // get_node<T>() are for, and they check.
    template<typename D, typename = std::enable_if_t<std::is_base_of<N, D>::value>>
    NodeRef(const NodeRef<D>& derived) : id_(derived.id()) {}

    // The static_cast is sound because three things hold at once. The id was
    // captured from an N*, so the object was an N at capture. A resolving id is
    // that same object, because unregister_node() zeroes the generation and
    // register_node() hands out a fresh monotonic one, so a reused address cannot
    // answer. And an object's dynamic type cannot change over its lifetime.
    //
    // That chain is also why there is no watch(): a mistyped reference is a
    // compile error here rather than a runtime check, and it is the absence of an
    // untyped capture path that keeps the cast above honest. Give this class back
    // a Node* parameter and the static_cast silently becomes unsound.
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
    The Node member templates whose bodies hand back a NodeRef live here, not in
    Nodes/Node.hpp. Returning one by value needs the complete type, and Node.hpp
    cannot include this header - NodeRef.hpp includes Node.hpp - so they are
    declared up there and defined down here. Same arrangement
    ScriptRef.hpp uses for attach_script().

    Node::get_script_as<S>() is the mirror case and lives in ScriptRef.hpp,
    because its return type is a ScriptRef.
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
    // A miss is an empty ref, and the NodeRef(N*) constructor cannot be handed
    // the nullptr resolve_path returns.
    T* found = static_cast<T*>(resolve_path(path));
    return found != nullptr ? NodeRef<T>(found) : NodeRef<T>();
}

} // namespace guayaba

#endif // GUAYABA_CORE_NODEREF_HPP