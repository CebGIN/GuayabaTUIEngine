#include <guayaba/Core/NodeRegistry.hpp>

namespace guayaba::detail {

NodeRegistry& NodeRegistry::instance() {
    // Deliberately leaked: a node in static storage would reach a destroyed
    // registry from its destructor.
    static NodeRegistry* registry = new NodeRegistry();
    return *registry;
}

uint64_t NodeRegistry::register_node(Node* node) {
    uint32_t slot;
    if (!free_slots_.empty()) {
        slot = free_slots_.back();
        free_slots_.pop_back();
    } else {
        slot = static_cast<uint32_t>(slots_.size());
        slots_.push_back(Slot{});
    }

    // 0 marks "empty", so it is skipped in both directions.
    if (next_generation_ == 0) next_generation_ = 1;
    const uint32_t generation = next_generation_++;

    slots_[slot].node = node;
    slots_[slot].generation = generation;
    live_count_++;

    return (static_cast<uint64_t>(generation) << SLOT_BITS) | slot;
}

void NodeRegistry::unregister_node(uint64_t id) {
    const uint32_t slot = static_cast<uint32_t>(id & SLOT_MASK);
    if (slot >= slots_.size()) return;

    // Idempotent: a second unregister finds nothing to clear.
    if (slots_[slot].node == nullptr) return;

    slots_[slot].node = nullptr;
    slots_[slot].generation = 0; // every outstanding id for this slot now mismatches
    free_slots_.push_back(slot);
    live_count_--;
}

Node* NodeRegistry::resolve(uint64_t id) const {
    if (id == INVALID_ID) return nullptr;

    const uint32_t slot = static_cast<uint32_t>(id & SLOT_MASK);
    if (slot >= slots_.size()) return nullptr;

    const uint32_t generation = static_cast<uint32_t>(id >> SLOT_BITS);
    if (slots_[slot].generation != generation) return nullptr;

    return slots_[slot].node;
}

size_t NodeRegistry::live_count() const {
    return live_count_;
}

} // namespace guayaba::detail