#ifndef GUAYABA_CORE_NODEREGISTRY_HPP
#define GUAYABA_CORE_NODEREGISTRY_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

namespace guayaba {

class Node;

namespace detail {

/*
    Side table mapping a packed uint64_t id to a live Node pointer.

    WHY THIS EXISTS
    Raw Node* handles are the fast path for in-subtree access, and they are not
    safe on their own: the tree frees nodes, so any handle held across a free
    dangles. This table is what lets a cheap handle detect that.

    THE ID
    An id packs a slot index and a generation into one integer:

        [ generation : 32 ][ slot : 32 ]

    Resolving is an array index plus one comparison. unregister_node() zeroes the
    slot's generation, so every id ever handed out for that slot stops matching
    immediately. Because the generation comes from a monotonic counter that never
    repeats, a handle minted before the slot was freed stays dead even after
    another node is constructed into the same address. Without the generation a
    handle would silently resolve to the wrong node instead of to nothing, which
    is a wrong-behaviour bug rather than a crash.

    Same shape Godot uses for ObjectDB (core/object/object.cpp), except Godot
    threads its free list through the slots' unused bitfields. We keep an
    explicit free list: a few hundred nodes do not justify the trickery.

    COST
    One vector push/pop per node construction and destruction. Nothing per frame.

    THREADING
    Single-threaded by assumption, deliberately: the engine runs exactly one loop.
    There is no lock here. If a worker thread is ever added, this class must grow
    one (Godot guards ObjectDB with a SpinLock for exactly that reason), or nodes
    deleted off-thread will race with resolution here.

    LIFETIME
    instance() returns a leaked singleton. Nodes may live in static storage, and a
    destroyed registry would be reached by their destructors.
*/
class NodeRegistry {
public:
    static NodeRegistry& instance();

    NodeRegistry(const NodeRegistry&) = delete;
    NodeRegistry& operator=(const NodeRegistry&) = delete;

    // Id 0 is the empty handle and is never handed out.
    static constexpr uint64_t INVALID_ID = 0;

    uint64_t register_node(Node* node);

    // Idempotent only via Node's destructor guard; passing an unknown id is a
    // no-op rather than an error, since ids are only ever derived from a Node.
    void unregister_node(uint64_t id);

    // nullptr once the node has been freed, even if its slot was reused.
    Node* resolve(uint64_t id) const;

    // Number of nodes constructed but not yet destroyed. Non-zero after the
    // scene is torn down means something leaked.
    size_t live_count() const;

private:
    NodeRegistry() = default;

    static constexpr uint32_t SLOT_BITS = 32;
    static constexpr uint64_t SLOT_MASK = (1ull << SLOT_BITS) - 1;

    struct Slot {
        Node* node = nullptr;
        uint32_t generation = 0;
    };

    std::vector<Slot> slots_;
    std::vector<uint32_t> free_slots_;
    uint32_t next_generation_ = 1;
    size_t live_count_ = 0;
};

} // namespace detail

} // namespace guayaba

#endif // GUAYABA_CORE_NODEREGISTRY_HPP