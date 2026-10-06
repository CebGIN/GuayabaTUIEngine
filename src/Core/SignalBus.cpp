#include <guayaba/Core/SignalBus.hpp>

namespace guayaba {

// Leaked on purpose, like detail::NodeRegistry: the bus has to outlive whatever is
// still emitting from a destructor or during static teardown, and a destroyed bus
// would leave every live emitter pointing at freed storage.
SignalBus& SignalBus::get_instance() {
    // Deliberately leaked, for the reason NodeRegistry::instance() is: the bus has
    // to outlive whatever is still emitting from a destructor.
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
