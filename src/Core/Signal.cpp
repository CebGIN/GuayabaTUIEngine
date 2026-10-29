#include <guayaba/Core/Signal.hpp>
#include <guayaba/Nodes/Node.hpp>

namespace guayaba::detail {

namespace {

// 0 is NodeRegistry::INVALID_ID and is never handed out, so a default-initialised
// id_ means "no connection". Monotonic, so a stale id never names a later one.
uint64_t connection_id_counter = 0;

} // namespace

uint64_t next_connection_id() {
    return ++connection_id_counter;
}

uint64_t node_id_of(Node* node) {
    return node != nullptr ? node->instance_id() : NodeRegistry::INVALID_ID;
}

} // namespace guayaba::detail
