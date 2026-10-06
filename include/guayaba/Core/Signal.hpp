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
    The shape every Signal<Args...> presents to the name -> signal map.

    WHY THE EARS EXISTS
    A node holds its signals in one heterogeneous map, so the map cannot be typed:
    "danio" is Signal<int> and "murio" is Signal<void()> and both live under the
    same owner. So the STORAGE is erased and the PUBLIC SURFACE is not - connect()
    and emit() stay templated on the real argument types, which is what lets a
    reactor be an ordinary function or lambda with ordinary parameters.

    The consequence worth stating: a reactor whose parameters do not match is a
    compile error at the connect call rather than a runtime throw at the emit.
    Nothing above this line ever sees the arguments, and nothing below it ever
    has to name them.

    It also carries signature(), so "you declared this name as void(int) and then
    asked for it as void(double)" can say so instead of reporting a bad cast.
*/
class SignalBase {
public:
    virtual ~SignalBase() = default;

    // "void(int, char)" and friends, built once per instantiation.
    virtual const char* signature() const = 0;

    virtual size_t reactor_count() const = 0;
    virtual bool is_connected(uint64_t connection_id) const = 0;

    // Returns whether it changed anything, so an id nobody holds is an answer
    // rather than a failure.
    virtual bool disconnect(uint64_t connection_id) = 0;

    virtual void clear() = 0;
};

// Monotonic, and 0 is never handed out, so a default-initialised id is invalid.
uint64_t next_connection_id();

// Declared here and defined in src/Core/Signal.cpp so that this header never needs
// Node to be a complete type. Taking the pointer by value is the whole trick: a
// declaration does not dereference it.
uint64_t node_id_of(Node* node);

// The node that OWNS A REACTOR, when there is one to ask.
//
// A script's owner is always its own node, so a reactor written as a method of a
// script is owned by that script's node - never by whichever node happens to hold
// the signal. Getting that backwards is a use-after-free rather than a leak: the
// signal outlives the reactor, so the reactor keeps being called after the script
// it belongs to has been destroyed.
//
// fallback is what to do when there is no owner to ask: the node whose own signal
// is being connected to, or INVALID_ID on the bus where no such node is in hand.
// Anything with get_owner() overrides it and ignores the fallback.
template<typename T, typename = void>
struct reactor_owner {
    static uint64_t of(T*, uint64_t fallback) { return fallback; }
};

template<typename T>
struct reactor_owner<T, std::void_t<decltype(std::declval<T&>().get_owner())>> {
    static uint64_t of(T* subject, uint64_t) { return node_id_of(subject->get_owner()); }
};

// Builds "void(int, char)". The two packs expand in lockstep, which is why this
// works at all: Args and I are the same length by construction.
template<typename T>
std::string type_name() {
    return typeid(T).name();
}

template<typename... Args, std::size_t... I>
std::string make_signature(std::index_sequence<I...>) {
    // Collected first rather than joined in place: an empty pack then produces
    // "void()" through the same code path, with no separator logic that only runs
    // when there is at least one argument.
    const std::vector<std::string> parts = { type_name<Args>()... };

    std::string out = "void(";
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i != 0) out += ", ";
        out += parts[i];
    }
    out += ")";
    return out;
}

// Turns any callable into the std::function a signal stores, rejecting one whose
// parameters do not match at compile time.
//
// WHY NOT JUST TAKE std::function<void(Args...)> AS THE PARAMETER
// Because a parameter pack there is a deduced context, so the compiler tries to
// deduce the signal's arguments out of the callable itself - and a lambda never
// matches that deduction, not even when the arguments are spelled out at the call
// site, because the deduction is attempted anyway. So the callable arrives as a
// forwarding reference and the std::function is built here, where the types are
// already known.
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
    A named event with a list of reactors. emit(args) calls every reactor that is
    still connected; connect adds one.

        node->connect_signal<int>("danio", [this](int d){ vida -= d; });
        node->emit_signal("danio", 7);

    WHO OWNS A REACTOR
    Every connection records the id of the node the reactor belongs to, which is
    the node you connected from. Before each call that id is resolved against
    NodeRegistry; if it no longer resolves, the reactor is dropped instead of
    called.

    That check is the whole of the auto-disconnect guarantee, and it rests on one
    invariant that already exists: ~Node calls unregister_node() before any member
    is destroyed, and every destruction path in the engine arrives there. So the
    id starts failing at the exact moment the reactor's owner becomes unreachable,
    which is precisely when calling it would be undefined behaviour. Nothing
    extra is registered, indexed or destroyed - the tree's own id table does the
    work. See Core/NodeRegistry.hpp.

    Dropping is lazy in the sense that it costs nothing until something emits: a
    connection to a dead owner lingers in the list, and the next emit on that
    signal prunes it. So a frame where nothing is emitted pays nothing at all.

    A connection made with an ownerless connect() has INVALID_ID as its owner and
    is never dropped automatically. That is the escape hatch for a reactor that
    owns nothing, and the price is that disconnecting it is your job.

    REENTRANCY
    emit() walks a snapshot of the reactor list, so a reactor may connect,
    disconnect or free nodes while it runs. The consequences are fixed rather
    than incidental:

      - Every PERSISTENT reactor connected when emit() began still runs, even if
        an earlier one disconnected it. There is no "cancelled" flag, matching
        Godot.
      - A reactor connected during an emit does NOT run for that emit.
      - A once-reactor is removed BEFORE being called, so a throwing reactor
        cannot leave it armed to fire again - and a reactor earlier in the same
        walk that re-emitted cannot fire it a second time.

    DEAD OWNERS
    The liveness check happens once, at the top of emit(), and the dead entries
    are removed there too. That placement is not incidental: a reactor can free
    the very Signal this method is running in - clear_signals(), SignalBus::clear(),
    dropping the unique_ptr that owns it - so anything touched after a reactor
    returns would be a use-after-free. Compacting first means nothing is.

    The cost is one registry resolve per reactor per emit, plus one compaction
    pass. For an event fired a handful of times a frame that is the right trade
    for not having to think about reentrancy; for a per-frame fan-out on a hot
    path, measure it.

    INHERITANCE
    detail::SignalBase is inherited publicly, which is what lets Node store a
    Signal<int> and a Signal<void()> in the same map. Nothing outside the engine
    ever names SignalBase, so the promotion to a common base costs nothing that
    can be observed.
*/
template<typename... Args>
class Signal : public detail::SignalBase {
public:
    using reactor_type = std::function<void(Args...)>;

    // Adds a reactor and returns the id that disconnects it.
    //
    // owner_id defaults to INVALID_ID, meaning "nobody will drop this for you".
    // The owner-aware entry point is Node::connect_signal(), which fills it in
    // with the connecting node's own id - that is the one you normally want, and
    // it is what a reactor written inside a Script gets automatically.
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

    // Calls every live reactor, forwarding the arguments unchanged. A reactor
    // that does not match this signature does not compile here, which is the
    // point of keeping the storage erased and the surface typed.
    void emit(Args... args) {
        // Pruned FIRST, and never afterwards. A reactor may call clear_signals(),
        // drop the node that owns this signal, or reset the unique_ptr that owns
        // it - any of which frees the object this method is a member of. So the
        // compaction happens while `this` is still known good, and nothing below
        // touches the signal after a reactor has run. That is also what makes
        // is_connected() and reactor_count() honest: they are asked after an emit,
        // and by then the dead entries are already gone.
        prune_dead();

        // Copied because a reactor is allowed to mutate this list, and the walk
        // has to survive it. Connections are a handful of entries in the shapes
        // this gets used for.
        const std::vector<Connection> snapshot = connections_;

        for (const Connection& connection : snapshot) {
            // Once more, because a reactor earlier in this same walk may have
            // emitted again and consumed it. Without the check a reentrant emit
            // would fire it twice.
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

    // One array index plus a compare. INVALID_ID is 0 and is never registered,
    // so the ownerless case never resolves at all.
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
    The name -> signal map, shared by Node and SignalBus so that "declare on
    demand", "the name is not declared", and "the name has another signature" all
    mean exactly one thing wherever a signal lives.

    owner_label is only ever used to build the message, and it is what tells the
    two cases apart in a throw: which node, or the bus.
*/
class SignalTable {
public:
    // nullptr rather than throwing: the "is it there, is it this shape" question
    // is a query, the same way get_script_as<S>() is.
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

    // Declares on demand. Re-declaring the same shape is not an error and not a
    // second signal: it hands back the existing one, so callers that race to
    // declare do not overwrite each other's reactors. A different shape cannot be
    // reconciled with a live list of reactors, so that throws.
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

        // Allocated before insertion and referred to afterwards: the pointee is
        // stable across the move into the map, so returning it is safe.
        std::unique_ptr<detail::SignalBase> owned(new Signal<Args...>());
        Signal<Args...>* typed = static_cast<Signal<Args...>*>(owned.get());
        entries_.emplace(signal_name, std::move(owned));
        return *typed;
    }

    // nullptr when the name is not declared at all, which is what lets
    // "declared with another signature" be a different message from "not declared".
    const char* signature_of(const std::string& signal_name) const {
        const auto entry = entries_.find(signal_name);
        return entry == entries_.end() ? nullptr : entry->second->signature();
    }

    // The bus's only handle on a connection is the id, because a bus signal is named
    // by whoever emits it and not by whoever wants to disconnect from it.
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