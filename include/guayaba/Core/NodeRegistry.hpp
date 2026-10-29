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

    An id packs a slot index and a generation into one integer:

        [ generation : 32 ][ slot : 32 ]

    Resolving is an array index plus one comparison. unregister_node() zeroes the
    slot's generation, so every id handed out for that slot stops matching. The
    generation comes from a monotonic counter, so a handle stays dead even after
    another node is built into the same address.

    One vector push/pop per node construction and destruction, nothing per frame.
    Single-threaded by assumption: there is no lock. instance() returns a leaked
    singleton, because nodes may live in static storage.
*/
class NodeRegistry {
public:
    static NodeRegistry& instance();

    NodeRegistry(const NodeRegistry&) = delete;
    NodeRegistry& operator=(const NodeRegistry&) = delete;

    // Id 0 is the empty handle and is never handed out.
    static constexpr uint64_t INVALID_ID = 0;

    uint64_t register_node(Node* node);

    // An unknown id is a no-op; ids only ever come from a Node.
    void unregister_node(uint64_t id);

    // nullptr once the node has been freed, even if its slot was reused.
    Node* resolve(uint64_t id) const;

    // Whether that id still names the same live node. Unlike resolve(), a fresh
    // node built into a freed address does not count. Use this after calling out
    // to code that could have destroyed the node.
    bool is_live(uint64_t id) const;

    // Nodes constructed but not yet destroyed.
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