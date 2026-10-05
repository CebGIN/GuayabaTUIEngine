#ifndef GUAYABA_SRC_CORE_DELETIONQUEUE_HPP
#define GUAYABA_SRC_CORE_DELETIONQUEUE_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <guayaba/Core/NodeRegistry.hpp>

namespace guayaba {

class Node;

namespace detail {

/*
    The list of nodes waiting to be destroyed, drained once per frame by the main
    loop. Delayed destruction buys reentrancy safety - a click handler can remove a
    node without pulling it out from under the loop iterating the tree. It is not
    what makes a raw pointer safe afterwards; that is NodeRef.

    Two containers, because there are two ways to leave:

      - schedule()          the node stays parented, so its parent's unique_ptr
                            still owns it and only the destruction is deferred.
                            All that is needed here is the id.
      - enqueue_detached()  the caller already unparented it, which handed us
                            the ownership, so it has to be held as unique_ptr.

    Recording an id instead of a pointer is what keeps the bookkeeping trivial.
    Resolving it at drain time answers the two questions a stored pointer cannot:
    a node already destroyed inside an ancestor's subtree resolves to nothing and
    is skipped (so nested queue_free() needs no cancelling), and a node whose
    remove_child() the consumer has since claimed has no parent and must not be
    deleted at all.

    Godot stores ObjectIDs for the same reason (scene/main/scene_tree.cpp). It
    never has to solve the ownership clash on the detached path because it uses
    raw pointers throughout; keeping the parent as owner until the drain is what
    avoids it here.

    Internal header: not installed, not part of the public API. The main loop owns
    the drain schedule.
*/
struct PendingDeletion {
    std::vector<uint64_t> scheduled_ids;
    std::vector<std::unique_ptr<Node>> detached;
};

inline PendingDeletion& pending_deletion() {
    // Function-local static rather than a global: one instance across
    // translation units, and constructed on first use.
    static PendingDeletion state;
    return state;
}

// Deferred destruction of a node that stays parented until the drain.
inline void schedule_deletion(Node* node) {
    if (node != nullptr) pending_deletion().scheduled_ids.push_back(node->instance_id());
}

// Deferred destruction of a node whose ownership we have already taken.
inline void enqueue_detached_deletion(std::unique_ptr<Node> node) {
    if (node) pending_deletion().detached.push_back(std::move(node));
}

inline size_t pending_deletion_count() {
    return pending_deletion().scheduled_ids.size() + pending_deletion().detached.size();
}

inline void drain_pending_deletions() {
    PendingDeletion& state = pending_deletion();

    // Scheduled ids first, so a queue_free()d node goes before anything detached
    // around it.
    while (!state.scheduled_ids.empty()) {
        const uint64_t id = state.scheduled_ids.back();
        state.scheduled_ids.pop_back();

        Node* node = NodeRegistry::instance().resolve(id);
        if (node == nullptr) continue;        // already gone inside an ancestor
        if (node->get_parent() == nullptr) continue; // consumer owns it now

        // Still owned by its parent's unique_ptr; reclaiming it here is what
        // actually destroys it, subtree and all.
        std::unique_ptr<Node> owned = node->get_parent()->remove_child(node);
    }

    while (!state.detached.empty()) {
        std::unique_ptr<Node> node = std::move(state.detached.back());
        state.detached.pop_back();
        node.reset();
    }
}

} // namespace detail

} // namespace guayaba

#endif // GUAYABA_SRC_CORE_DELETIONQUEUE_HPP