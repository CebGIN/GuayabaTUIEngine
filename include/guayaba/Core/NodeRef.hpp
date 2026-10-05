#ifndef GUAYABA_CORE_NODEREF_HPP
#define GUAYABA_CORE_NODEREF_HPP

#include <cassert>
#include <cstdint>

#include <guayaba/Core/NodeRegistry.hpp>
#include <guayaba/Nodes/Node.hpp>

namespace guayaba {

/*
    Non-owning, dangling-safe handle to a Node. Eight bytes: a packed id, no
    pointer, so it cannot dangle and cannot silently alias a node that was
    constructed into the same address afterwards.

    WHEN YOU NEED THIS
    Raw T* is the right handle for anything inside the subtree you built: an
    ancestor of the holder, or anything the holder's subtree owns. That
    containment is structural, because the tree owns its children outright.

    A reference that crosses that boundary cannot be structural. The clearest
    case is a node handed to a builder as an argument: the caller may free that
    subtree at any time, and nothing in the receiving subtree keeps it alive.
    Store a NodeRef there and test it.

        class Follower : public Script {
            NodeRef<Node2D> target;   // lives in someone else's subtree
            void on_process(double) override {
                if (auto t = target.get()) t->set_local_position(Input::mouse_pos);
            }
        };

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
template<typename T>
class NodeRef {
public:
    NodeRef() = default;

    // Watches a node. If raw is already dead the result starts invalid, so a
    // handle can never be born holding a lie.
    explicit NodeRef(Node* raw) { watch(raw); }

    void watch(Node* raw) {
        id_ = raw ? raw->instance_id() : detail::NodeRegistry::INVALID_ID;
    }

    void reset() { id_ = detail::NodeRegistry::INVALID_ID; }

    // dynamic_cast rather than static_cast: the watch() argument is a Node*, so
    // a mistyped handle degrades to nullptr instead of a bad cast.
    T* get() const {
        Node* node = detail::NodeRegistry::instance().resolve(id_);
        return node ? dynamic_cast<T*>(node) : nullptr;
    }

    bool is_valid() const { return get() != nullptr; }

    explicit operator bool() const { return is_valid(); }

    T* operator->() const {
        T* node = get();
        assert(node != nullptr && "NodeRef::operator-> on a dead reference.");
        return node;
    }

    uint64_t id() const { return id_; }

private:
    uint64_t id_ = detail::NodeRegistry::INVALID_ID;
};

} // namespace guayaba

#endif // GUAYABA_CORE_NODEREF_HPP