#ifndef GUAYABA_CORE_SIGNALBUS_HPP
#define GUAYABA_CORE_SIGNALBUS_HPP

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include <guayaba/Core/NodeRegistry.hpp>
#include <guayaba/Core/Signal.hpp>

namespace guayaba {

class Node;

/*
    Engine-wide signals, for the things that are not one node's business: input,
    scene changes, a game state that any part of a scene may want to hear about.

        bus().connect_script_signal("mouse_clicked", this, &MyScript::on_mouse_clicked);

    WHAT THIS IS NOT
    Not a replacement for Node signals. If a signal has exactly one meaningful
    owner, that owner is a node and the signal belongs to it, reachable by path:

        player->emit_signal("danio", 7);

    Reached that way, the emitter and the listener are both named in one place, and
    the emitter cannot outlive its listeners by accident. Use the bus for signals
    that are genuinely ambient, or when the emitter does not know who is listening.

    LIFETIME
    A single leaked instance, for the same reason detail::NodeRegistry's is.

    The connections themselves are owned by nothing, which is the one thing about
    the bus that is not automatic. A connection given an owner node is dropped by
    that node's id in the same way a node signal's is; a connection given none
    lives until disconnect_signal(), or until clear().

    THREADING
    Single-threaded by assumption, like the rest of the engine.
*/
class SignalBus {
public:
    static SignalBus& get_instance();

    SignalBus(const SignalBus&) = delete;
    SignalBus& operator=(const SignalBus&) = delete;

    // Declares the signal on demand, then emits it. Same reentrancy rules as any
    // other signal: the reactor list is snapshotted, and a reactor connected
    // during this emit does not run for it.
    template<typename... Args>
    void emit_signal(const std::string& signal_name, Args&&... args) {
        declare<std::decay_t<Args>...>(signal_name).emit(std::forward<Args>(args)...);
    }

    // Adds a reactor and returns the id that disconnects it.
    //
    // owner is the node the reactor belongs to, and passing it is what makes the
    // connection disappear when that node is destroyed. It defaults to nullptr for
    // a reactor that owns nothing at all - a free function, a lambda over a value
    // - which then has to be disconnected by hand.
    template<typename... Args, typename F>
    uint64_t connect_signal(const std::string& signal_name, F&& reactor,
                            Node* owner = nullptr, bool once = false) {
        return declare<Args...>(signal_name).connect(
            detail::make_reactor<Args...>(std::forward<F>(reactor)),
            detail::node_id_of(owner), once);
    }

    // The form to use from inside a Script when the reactor is a method of that
    // script: the arguments come from the method's own signature, so nothing is
    // spelled twice, and there is no lambda capturing `this` to get wrong.
    //
    //     void on_mouse_clicked(int button, Vec2 pos);
    //     ...
    //     bus().connect_script_signal("mouse_clicked", this, &MyScript::on_mouse_clicked);
    //
    // The owner is read off the script itself - a script's owner is always its
    // own node - so the connection dies with that node and nothing has to be
    // disconnected by hand. C is not constrained to be a Script: a plain class
    // with the right member works, and gets an ownerless connection, which is the
    // honest answer for an object no node owns.
    template<typename R, typename C, typename... Args>
    uint64_t connect_script_signal(const std::string& signal_name,
                                   C* reactor,
                                   R (C::*member)(Args...),
                                   bool once = false) {
        if (reactor == nullptr) {
            throw std::invalid_argument("SignalBus::connect_script_signal: null reactor.");
        }
        return declare<std::decay_t<Args>...>(signal_name).connect(
            [reactor, member](std::decay_t<Args>... args) {
                (reactor->*member)(std::forward<decltype(args)>(args)...);
            },
            detail::reactor_owner<C>::of(reactor, detail::NodeRegistry::INVALID_ID), once);
    }

    // A query, false rather than throwing: an emitter asks this so that a signal
    // nobody listens to costs one map lookup instead of an allocation.
    template<typename... Args>
    bool has_signal(const std::string& signal_name) const {
        return signals.find<std::decay_t<Args>...>(signal_name) != nullptr;
    }

    // The same question with the shape ignored: true if the name is declared at
    // all, whatever its signature.
    //
    // This is the one an ENGINE emitter wants. has_signal<Args...> is also false
    // when the name exists with a different signature, and using that as an emit
    // guard turns one wrong guess into a signal that silently never fires for
    // the rest of the process - the guard itself is what makes it silent.
    bool is_declared(const std::string& signal_name) const {
        return signals.signature_of(signal_name) != nullptr;
    }

    // The signal itself, for a caller that wants to count reactors or disconnect
    // one directly. Throws if the name is not declared with this signature.
    template<typename... Args>
    Signal<Args...>& signal(const std::string& signal_name) {
        Signal<Args...>* found = signals.find<Args...>(signal_name);
        if (found == nullptr) throw not_declared(signal_name);
        return *found;
    }

    // True when a connection with this id is still live. The cheap way to find out
    // whether an ownerless connection is still there.
    bool is_connected(uint64_t connection_id) const;

    // False if nobody held that id, so a double disconnect is a no-op rather than
    // an error.
    bool disconnect_signal(uint64_t connection_id);

    // Drops every connection and every declared signal. The engine does not call
    // this for you: a scene change leaves the bus alone, because a connection
    // made for the next scene may already be there. Call it where you mean it.
    void clear() { signals.clear(); }

private:
    SignalBus() = default;

    template<typename... Args>
    Signal<Args...>& declare(const std::string& signal_name) {
        return signals.declare<Args...>(signal_name, "The global signal bus");
    }

    std::logic_error not_declared(const std::string& signal_name) const {
        const char* existing = signals.signature_of(signal_name);
        if (existing != nullptr) {
            return std::logic_error(std::string("SignalBus::signal: the bus declares \"")
                + signal_name + "\" as " + existing + ", not the requested signature.");
        }
        return std::logic_error(std::string("SignalBus::signal: the bus declares no signal named \"")
            + signal_name + "\". connect_signal and emit_signal declare one on demand.");
    }

    SignalTable signals;
};

} // namespace guayaba

#endif // GUAYABA_CORE_SIGNALBUS_HPP