#include <guayaba/Core/Signal.hpp>
#include <guayaba/Nodes/Node.hpp>

namespace guayaba::detail {

namespace {

// 0 is NodeRegistry::INVALID_ID and is never handed out as a connection id, so a
// default-initialised id_ is unambiguously "no connection". Monotonic rather than
// reused, so a stale id can never name a different connection later.
uint64_t connection_id_counter = 0;

} // namespace

uint64_t next_connection_id() {
    return ++connection_id_counter;
}

uint64_t node_id_of(Node* node) {
    return node != nullptr ? node->instance_id() : NodeRegistry::INVALID_ID;
}

} // namespace guayaba::detail