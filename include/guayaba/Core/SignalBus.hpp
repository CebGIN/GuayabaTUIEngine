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
    Engine-wide signals, for things that are not one node's business: input, scene
    changes, a game state any part of a scene may want to hear about.

        bus().connect_script_signal("mouse_clicked", this, &MyScript::on_mouse_clicked);

    A single leaked instance, like detail::NodeRegistry. A connection given an
    owner node is dropped when that node dies; one given none lives until
    disconnect_signal() or clear(). Single-threaded by assumption.
*/
class SignalBus {
public:
    static SignalBus& get_instance();

    SignalBus(const SignalBus&) = delete;
    SignalBus& operator=(const SignalBus&) = delete;

    // Declares the signal on demand, then emits it. See Signal::emit.
    template<typename... Args>
    void emit_signal(const std::string& signal_name, Args&&... args) {
        declare<std::decay_t<Args>...>(signal_name).emit(std::forward<Args>(args)...);
    }

    // Adds a reactor and returns the id that disconnects it.
    //
    // owner makes the connection disappear when that node is destroyed. It
    // defaults to nullptr for a reactor that owns nothing, which then has to be
    // disconnected by hand.
    template<typename... Args, typename F>
    uint64_t connect_signal(const std::string& signal_name, F&& reactor,
                            Node* owner = nullptr, bool once = false) {
        return declare<Args...>(signal_name).connect(
            detail::make_reactor<Args...>(std::forward<F>(reactor)),
            detail::node_id_of(owner), once);
    }

    // Connects a method as the reactor, with the argument types taken from the
    // method's own signature.
    //
    //     void on_mouse_clicked(int button, Vec2 pos);
    //     bus().connect_script_signal("mouse_clicked", this, &MyScript::on_mouse_clicked);
    //
    // The owner is read off the script, so the connection dies with its node. A
    // plain class with the right member also works and gets an ownerless
    // connection.
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

    // True when the name is declared with this signature.
    template<typename... Args>
    bool has_signal(const std::string& signal_name) const {
        return signals.find<std::decay_t<Args>...>(signal_name) != nullptr;
    }

    // True when the name is declared at all, whatever its signature. This is the
    // guard an engine emitter wants: has_signal<Args...> is also false when the
    // name exists with a different signature, so using it as an emit guard would
    // silently drop the signal.
    bool is_declared(const std::string& signal_name) const {
        return signals.signature_of(signal_name) != nullptr;
    }

    // The named signal. Throws if it is not declared with this signature.
    template<typename... Args>
    Signal<Args...>& signal(const std::string& signal_name) {
        Signal<Args...>* found = signals.find<Args...>(signal_name);
        if (found == nullptr) throw not_declared(signal_name);
        return *found;
    }

    // True when a connection with this id is still live.
    bool is_connected(uint64_t connection_id) const;

    // False if nobody held that id, so a double disconnect is a no-op.
    bool disconnect_signal(uint64_t connection_id);

    // Drops every connection and every declared signal. A scene change does not
    // call this, so a connection made for the next scene survives.
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