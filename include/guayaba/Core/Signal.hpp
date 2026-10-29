#ifndef GUAYABA_CORE_SIGNAL_HPP
#define GUAYABA_CORE_SIGNAL_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

#include <guayaba/Core/NodeRegistry.hpp>

namespace guayaba {

class Node;

namespace detail {

/*
    Type-erased base of every Signal<Args...>, so signals of different signatures
    can live in one name -> signal map. connect() and emit() stay templated on the
    real argument types; only the storage is erased.

    signature() reports "void(int, char)" so a name declared with one signature and
    requested with another can say so.
*/
class SignalBase {
public:
    virtual ~SignalBase() = default;

    // e.g. "void(int, char)", built once per instantiation.
    virtual const char* signature() const = 0;

    virtual size_t reactor_count() const = 0;
    virtual bool is_connected(uint64_t connection_id) const = 0;

    // True if a connection with this id existed and was removed.
    virtual bool disconnect(uint64_t connection_id) = 0;

    virtual void clear() = 0;
};

// Monotonic, and 0 is never handed out, so a default-initialised id is invalid.
uint64_t next_connection_id();

// Declared here and defined in Signal.cpp so this header does not need Node to be
// complete.
uint64_t node_id_of(Node* node);

// The node id that owns a reactor. A type with get_owner() (a Script) reports its
// own node; anything else returns `fallback` unchanged.
template<typename T, typename = void>
struct reactor_owner {
    static uint64_t of(T*, uint64_t fallback) { return fallback; }
};

template<typename T>
struct reactor_owner<T, std::void_t<decltype(std::declval<T&>().get_owner())>> {
    static uint64_t of(T* subject, uint64_t) { return node_id_of(subject->get_owner()); }
};

// Builds the signature string, e.g. "void(int, char)".
template<typename T>
std::string type_name() {
    return typeid(T).name();
}

template<typename... Args, std::size_t... I>
std::string make_signature(std::index_sequence<I...>) {
    // Collected first so an empty pack still goes through the same join below.
    const std::vector<std::string> parts = { type_name<Args>()... };

    std::string out = "void(";
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i != 0) out += ", ";
        out += parts[i];
    }
    out += ")";
    return out;
}

// Builds the std::function a signal stores from any callable, rejecting one whose
// parameters do not match at compile time.
//
// Takes the callable as a forwarding reference because a parameter pack in a
// std::function<void(Args...)> parameter is a deduced context, so a lambda would
// be matched against it instead of being converted.
template<typename... Args, typename F>
std::function<void(Args...)> make_reactor(F&& reactor) {
    static_assert(std::is_invocable_v<F&, Args...>,
        "This reactor cannot be called with the signal's arguments. Compare its parameter list"
        " with the signal's signature - remember that emit_signal decays what it is given, so a"
        " signal declared void(int) takes an int.");
    return std::function<void(Args...)>(std::forward<F>(reactor));
}

} // namespace detail

/*
    A named event with a list of reactors. connect() adds one and returns the id
    that disconnects it; emit(args) calls every reactor still connected.

        node->connect_signal<int>("danio", [this](int d){ vida -= d; });
        node->emit_signal("danio", 7);

    Each connection records the id of the node the reactor belongs to. Before each
    call that id is resolved against NodeRegistry and the reactor is dropped when
    it stops resolving, so a connection dies with its owner. A connection made
    with the default owner id (INVALID_ID) is never dropped automatically and must
    be disconnected by hand.

    emit() walks a snapshot of the reactor list, so a reactor may connect,
    disconnect or free nodes as it runs. A reactor connected during the emit does
    not run for it, and a once-reactor is removed before it is called. Dead owners
    are pruned at the top of emit(), before any reactor runs, because a reactor may
    free the Signal itself.
*/
template<typename... Args>
class Signal : public detail::SignalBase {
public:
    using reactor_type = std::function<void(Args...)>;

    // Adds a reactor and returns the id that disconnects it.
    //
    // owner_id is the node the reactor belongs to; it defaults to INVALID_ID,
    // meaning nobody drops this connection for you. Node::connect_signal() fills
    // in the connecting node's id.
    uint64_t connect(reactor_type reactor,
                     uint64_t owner_id = detail::NodeRegistry::INVALID_ID,
                     bool once = false) {
        if (!reactor) {
            throw std::invalid_argument("Signal::connect: null reactor.");
        }

        Connection connection;
        connection.id = detail::next_connection_id();
        connection.owner_id = owner_id;
        connection.once = once;
        connection.reactor = std::move(reactor);
        connections_.push_back(std::move(connection));
        return connections_.back().id;
    }

    // Calls every live reactor, forwarding the arguments unchanged.
    void emit(Args... args) {
        // Pruned first: a reactor may free this Signal, so nothing may touch
        // `this` after a reactor has run.
        prune_dead();

        // A copy, because a reactor may mutate this list while the walk runs.
        const std::vector<Connection> snapshot = connections_;

        for (const Connection& connection : snapshot) {
            // A reentrant emit may have consumed this once-reactor already.
            if (connection.once && !disconnect(connection.id)) continue;

            connection.reactor(std::forward<Args>(args)...);
        }
    }

    size_t reactor_count() const override { return connections_.size(); }

    bool is_connected(uint64_t connection_id) const override {
        for (const Connection& connection : connections_) {
            if (connection.id == connection_id) return true;
        }
        return false;
    }

    bool disconnect(uint64_t connection_id) override {
        for (auto it = connections_.begin(); it != connections_.end(); ++it) {
            if (it->id == connection_id) {
                connections_.erase(it);
                return true;
            }
        }
        return false;
    }

    void clear() override { connections_.clear(); }

private:
    struct Connection {
        uint64_t id = 0;
        uint64_t owner_id = detail::NodeRegistry::INVALID_ID;
        bool once = false;
        reactor_type reactor;
    };

    // INVALID_ID is 0 and never registered, so the ownerless case never resolves.
    static bool is_owner_dead(uint64_t owner_id) {
        if (owner_id == detail::NodeRegistry::INVALID_ID) return false;
        return detail::NodeRegistry::instance().resolve(owner_id) == nullptr;
    }

    void prune_dead() {
        connections_.erase(
            std::remove_if(connections_.begin(), connections_.end(),
                [](const Connection& connection) { return is_owner_dead(connection.owner_id); }),
            connections_.end());
    }

    const char* signature() const override {
        static const std::string built =
            detail::make_signature<Args...>(std::index_sequence_for<Args...>{});
        return built.c_str();
    }

    std::vector<Connection> connections_;
};

/*
    The name -> signal map, shared by Node and SignalBus.

    owner_label is used only to build error messages (the node's name or "The
    global signal bus").
*/
class SignalTable {
public:
    // nullptr when the name is absent or has another signature.
    template<typename... Args>
    Signal<Args...>* find(const std::string& signal_name) {
        const auto entry = entries_.find(signal_name);
        if (entry == entries_.end()) return nullptr;
        return dynamic_cast<Signal<Args...>*>(entry->second.get());
    }

    template<typename... Args>
    Signal<Args...>* find(const std::string& signal_name) const {
        const auto entry = entries_.find(signal_name);
        if (entry == entries_.end()) return nullptr;
        return dynamic_cast<Signal<Args...>*>(entry->second.get());
    }

    // Declares on demand. Re-declaring the same shape returns the existing signal;
    // a different shape throws.
    template<typename... Args>
    Signal<Args...>& declare(const std::string& signal_name, const std::string& owner_label) {
        const auto entry = entries_.find(signal_name);
        if (entry != entries_.end()) {
            Signal<Args...>* typed = dynamic_cast<Signal<Args...>*>(entry->second.get());
            if (typed == nullptr) {
                throw std::logic_error(owner_label + " declares the signal \"" + signal_name
                    + "\" as " + entry->second->signature() + ", which is not the requested signature.");
            }
            return *typed;
        }

        // The pointee is stable across the move into the map.
        std::unique_ptr<detail::SignalBase> owned(new Signal<Args...>());
        Signal<Args...>* typed = static_cast<Signal<Args...>*>(owned.get());
        entries_.emplace(signal_name, std::move(owned));
        return *typed;
    }

    // nullptr when the name is not declared at all.
    const char* signature_of(const std::string& signal_name) const {
        const auto entry = entries_.find(signal_name);
        return entry == entries_.end() ? nullptr : entry->second->signature();
    }

    // True when any declared signal holds this connection id.
    bool is_connected(uint64_t connection_id) const {
        for (const auto& entry : entries_) {
            if (entry.second->is_connected(connection_id)) return true;
        }
        return false;
    }

    bool disconnect(uint64_t connection_id) {
        for (auto& entry : entries_) {
            if (entry.second->disconnect(connection_id)) return true;
        }
        return false;
    }

    // Drops every reactor on every signal, and the signals themselves.
    void clear() { entries_.clear(); }

    size_t size() const { return entries_.size(); }

private:
    std::map<std::string, std::unique_ptr<detail::SignalBase>> entries_;
};

} // namespace guayaba

#endif // GUAYABA_CORE_SIGNAL_HPP