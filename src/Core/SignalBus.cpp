#include <guayaba/Core/SignalBus.hpp>

namespace guayaba {

// Leaked so the bus outlives whatever is still emitting from a destructor or
// during static teardown.
SignalBus& SignalBus::get_instance() {
    static SignalBus* instance = new SignalBus();
    return *instance;
}

bool SignalBus::is_connected(uint64_t connection_id) const {
    return signals.is_connected(connection_id);
}

bool SignalBus::disconnect_signal(uint64_t connection_id) {
    return signals.disconnect(connection_id);
}

} // namespace guayaba
