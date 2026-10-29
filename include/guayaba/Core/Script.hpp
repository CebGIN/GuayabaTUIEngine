#ifndef GUAYABA_CORE_SCRIPT_HPP
#define GUAYABA_CORE_SCRIPT_HPP

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include <guayaba/Core/InputEvent.hpp>
#include <guayaba/Core/SignalBus.hpp>
#include <guayaba/Nodes/Node.hpp>

namespace guayaba {

/*
    Behaviour attached to an existing Node without deriving from that node's
    class.

      - ScriptBase is what a Node stores and dispatches through. It knows the
        owner only as a Node*, so it works for any node.
      - Script<N> is what you derive from. It publishes node_type = N and keeps
        N* node, so callbacks touch the concrete node without a cast.

        class Blinker : public Script<Node2D> {
        public:
            double period = 0.5;
            double elapsed = 0.0;
            void on_process(double dt) override {
                elapsed += dt;
                if (elapsed >= period) {
                    elapsed = 0.0;
                    node->invert_colors();   // typed: no cast
                }
            }
        };
        ScriptRef<Node2D, Blinker> blink = body->attach_script<Blinker>();

    Derive from Script<N>, never from ScriptBase, which would have to cast back
    to anything interesting.

    The node owns its script and keeps the first one it is given until it is
    destroyed: there is no detach, and attaching a second script throws. Build
    the subtree and attach its scripts before handing it to a scene.

    Hooks, all no-ops by default:
      - on_attached / on_detached: the script is bound to its node / the node is
        destroyed.
      - on_enter_tree / on_exit_tree: the node joins or leaves the scene tree.
      - on_process: every frame while the node is in the tree.
      - on_input: during the tree walk; return true to consume the event.
      - on_unhandled_input: events that routing left over.

    Signals: declare_signal, emit_signal and connect_signal act on this script's
    own node; connect_script_signal targets another node. A connection made from
    a script is owned by this script's node, so it drops when that node dies. See
    Core/Signal.hpp.
*/
class ScriptBase {
public:
    virtual ~ScriptBase() = default;

    // The script is bound to its node / the node is being destroyed.
    virtual void on_attached() {}
    virtual void on_detached() {}

    // The node joins or leaves the scene tree.
    virtual void on_enter_tree() {}
    virtual void on_exit_tree() {}

    // Every frame, for as long as the node is in the tree.
    virtual void on_process(double delta_time) { (void)delta_time; }

    /*
        on_input runs during the tree walk, after this node's children have had
        the event; return true to consume it. on_unhandled_input runs afterwards
        for whatever routing left over. Both default to doing nothing.
    */
    virtual bool on_input(const InputEvent& event) { (void)event; return false; }
    virtual void on_unhandled_input(const InputEvent& event) { (void)event; }

    const std::string& get_script_name() const { return script_name; }
    void set_script_name(std::string name) { script_name = std::move(name); }

    // The owning node, valid while any script method is running. Script<N>
    // exposes the same pointer narrowed to N as `node`.
    Node* get_owner() const { return owner; }

    // Escape hatch for code written against ScriptBase. Script<N> already has
    // `node` typed.
    template<typename T>
    T* get_owner_as() const {
        static_assert(std::is_base_of<Node, T>::value, "T must be a Node type.");
        return dynamic_cast<T*>(owner);
    }

protected:
    friend class Node;

    // Only Node calls this, once per script. Virtual so Script<N> can build its
    // typed `node` pointer when Node binds through a ScriptBase*.
    virtual void bind_to(Node* host) { owner = host; }

    // --- Signals ------------------------------------------------------------
    //
    // bus, declare_signal, emit_signal and connect_signal act on this script's
    // own node with node-> filled in. connect_script_signal takes an explicit
    // target for a signal on another node. Protected because they require the
    // script to be bound; see owner_nonnull().

    // The bus, for the signals no single node owns.
    SignalBus& bus() const { return SignalBus::get_instance(); }

    // Declares or retrieves a signal on this script's own node.
    template<typename... Args>
    Signal<Args...>& declare_signal(const std::string& signal_name) {
        return owner_nonnull()->template declare_signal<Args...>(signal_name);
    }

    // Emits on this script's own node.
    template<typename... Args>
    void emit_signal(const std::string& signal_name, Args&&... args) {
        owner_nonnull()->template emit_signal<Args...>(signal_name, std::forward<Args>(args)...);
    }

    // Connects a reactor to a signal on this script's own node. Throws if the
    // node has no such signal. The reactor is owned by this script's node, so it
    // dies with the listener rather than with the emitter. Use
    // connect_script_signal below for a signal on another node.
    template<typename... Args, typename F>
    uint64_t connect_signal(const std::string& signal_name, F&& reactor, bool once = false) {
        Node* target = owner_nonnull();
        return target->template signal<Args...>(signal_name).connect(
            detail::make_reactor<Args...>(std::forward<F>(reactor)), owner->instance_id(), once);
    }

    // Connects a method of this script to a signal on `target`, with the argument
    // types taken from the method's signature.
    //
    //     connect_script_signal("danio", enemigo.get(), this, &MyScript::recibir_danio);
    //
    // The owner is read off this script, not off `target`, so the connection dies
    // with the listener. Throws when `target` has no such signal.
    template<typename R, typename C, typename... Args>
    uint64_t connect_script_signal(const std::string& signal_name,
                                   Node* target,
                                   C* reactor,
                                   R (C::*member)(Args...),
                                   bool once = false) {
        if (target == nullptr) {
            throw std::invalid_argument("ScriptBase::connect_script_signal: null target node.");
        }
        if (reactor == nullptr) {
            throw std::invalid_argument("ScriptBase::connect_script_signal: null reactor.");
        }
        return target->template signal<std::decay_t<Args>...>(signal_name).connect(
            [reactor, member](std::decay_t<Args>... args) {
                (reactor->*member)(std::forward<decltype(args)>(args)...);
            },
            detail::reactor_owner<C>::of(reactor, owner->instance_id()), once);
    }

private:
    // Throws when the script is not bound yet, which is the only way owner can be
    // null here.
    Node* owner_nonnull() const {
        if (owner == nullptr) {
            throw std::logic_error("ScriptBase: no signal call is valid before the script is bound"
                " to a node. Wire signals from on_attached or a later hook.");
        }
        return owner;
    }

    // The owning node, set once by bind_to() and valid until the node destroys
    // this script.
    Node* owner = nullptr;
    std::string script_name = "Unnamed Script";
};

/*
    A script that knows the concrete node type it belongs to.

    Inside a callback `node` is a ready-to-use N*, so reaching a property needs no
    cast. Binding a Script<N> to a node that is not an N throws
    std::invalid_argument from bind_to: the check is a runtime dynamic_cast
    because attach_script<T>() cannot know the type of the node calling it.
*/
template<typename N = Node>
class Script : public ScriptBase {
public:
    // Lets Node::attach_script<T>() check that T is a Script<N> and not a
    // ScriptBase; see has_node_type in Node.hpp.
    using node_type = N;

protected:
    // The owner as its concrete type, non-null while this script is bound.
    N* node = nullptr;

    void bind_to(Node* host) override {
        // Checked before touching any state, so a rejected bind leaves the
        // script unbound.
        N* typed = dynamic_cast<N*>(host);
        if (!typed) {
            throw std::invalid_argument(
                "Script<N>::bind_to: the host node is not of the node type this "
                "script was declared for.");
        }
        ScriptBase::bind_to(host);
        node = typed;
    }
};

} // namespace guayaba

#endif // GUAYABA_CORE_SCRIPT_HPP