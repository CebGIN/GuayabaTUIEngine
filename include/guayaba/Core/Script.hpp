#ifndef GUAYABA_CORE_SCRIPT_HPP
#define GUAYABA_CORE_SCRIPT_HPP

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include <guayaba/Core/SignalBus.hpp>
#include <guayaba/Nodes/Node.hpp>

namespace guayaba {

class ConsoleRenderer;

/*
    Behaviour that attaches to an existing Node without deriving from that
    node's C++ class. The idea is Godot's Script: bolt logic and your own state
    onto a prefab you did not write, without reimplementing its rendering.

    Two classes, in two layers:

      - ScriptBase  is what a Node stores and dispatches through. It knows the
        owner only as a Node*, so it is the type that works for any node.
      - Script<N>    is what you derive from. It names the type of node it
        belongs to and keeps a typed handle to it, so your callbacks touch
        properties directly instead of casting first.

        class Blinker : public Script<Node2D> {
        public:
            double period = 0.5;
            double elapsed = 0.0;
            void on_process(double dt) override {
                elapsed += dt;
                if (elapsed >= period) {
                    elapsed = 0.0;
                    node->invert_colors();   // typed: no cast, no qualifier
                }
            }
        };

        Node2D* body = ...;
        ScriptRef<Node2D, Blinker> blink = body->attach_script<Blinker>();
        blink.script()->period = 0.2;   // your own fields, through your own handle

    Derive from Script<N>, never from ScriptBase: a script written against
    ScriptBase has to cast its way back to anything interesting. Derive from
    Script<N> and the cast is already done.

    Neither class is abstract in the language sense - every hook below has a
    no-op default, so a draw-only script is not made to stub on_process. What is
    enforced is the contract: attach_script<T>() rejects an abstract T, because a
    hook you cannot override is a hook the engine can dispatch to nothing.

    THE SLOT IS SEALED
    A node keeps the first script it is given until it is destroyed. There is no
    detach, and attaching a second script throws, because swapping one out would
    leave every handle anyone held pointing at freed memory with nothing to test
    - the exact situation ScriptRef exists to make checkable. Build the subtree
    and attach the scripts before handing it to a scene; see Core/ScriptRef.hpp
    for what the seal buys and what depends on it.

    Two lifecycles live here, and they are deliberately not mixed:

      - The NODE's lifecycle -> on_enter_tree / on_exit_tree. Fired only when
        the node actually joins or leaves the scene tree.
      - The SCRIPT's lifecycle -> on_attached / on_detached. Fired when the
        script is bound to a node, and when that node is destroyed. Since
        binding only happens while building, on_attached is a build-time hook,
        not something the running tree triggers.

    SIGNALS
    Those hooks are the script's own: the engine knows to call them. A signal is
    the other direction - the script says who else may call into it, by name,
    with neither side holding a handle to the other:

        void on_attached() override {
            // this node's own signal
            connect_script_signal("danio", node, this, &Blinker::on_danio);
            // a signal on some other node, owned by this script
            connect_script_signal("murio", enemigo.get(), this, &Blinker::al_morir);
            // and one on the bus
            bus().connect_script_signal("mouse_clicked", this, &Blinker::on_click);
        }
        void on_danio(int d) { vida -= d; }

    Every one of those reads the owner off the script, so the connection is dropped
    when THIS node is destroyed and there is nothing to disconnect by hand. That
    matters most for the cross-node case, where the emitter normally outlives the
    listener: owning by the emitter would keep calling a script that is gone. The
    forwarders below are these calls with node-> already filled in. See
    Core/Signal.hpp.

    A script attached while the node is already live never sees on_enter_tree,
    because a live node cannot be given a new script. Ask for it yourself:

        void on_attached() override {
            if (node->is_in_tree()) on_enter_tree();
        }

    OWNERSHIP
    The node owns its script, and that is what makes the back reference cheap:
    while any method of yours is running, the node is guaranteed alive, because it
    owns the object you are executing on. So owner is a plain Node* - no lock, no
    reference count, no validity check per frame. Script<N>'s typed handle is the
    same pointer, narrowed once at bind time rather than per access.
*/
class ScriptBase {
public:
    virtual ~ScriptBase() = default;

    // Script lifecycle. Both are build-time hooks now: binding only happens
    // while assembling a subtree, and a node fires on_detached from its own
    // destructor, so it is the last thing your script ever sees.
    virtual void on_attached() {}
    virtual void on_detached() {}

    // Host node lifecycle. Fires only on real tree transitions.
    virtual void on_enter_tree() {}
    virtual void on_exit_tree() {}

    // Every frame, for as long as the node is in the tree.
    virtual void on_process(double delta_time) { (void)delta_time; }

    // After the node's own drawing and BEFORE its children, so that a child's
    // on_draw stays a genuine overlay on top of the parent's.
    virtual void on_draw(ConsoleRenderer& renderer) { (void)renderer; }

    const std::string& get_script_name() const { return script_name; }
    void set_script_name(std::string name) { script_name = std::move(name); }

    // Non-null for as long as this script exists, which is as long as its node.
    // Safe without a check for as long as this method is running, because the
    // node owns us. Inside Script<N> prefer node, which is the same pointer
    // already narrowed to N.
    Node* get_owner() const { return owner; }

    // Escape hatch for code written against ScriptBase. A Script<N> needs none
    // of this: node is already the concrete type.
    template<typename T>
    T* get_owner_as() const {
        static_assert(std::is_base_of<Node, T>::value, "T must be a Node type.");
        return dynamic_cast<T*>(owner);
    }

protected:
    friend class Node;

    // Only Node binds a script, and only once: the slot is sealed on the first
    // successful bind, so there is no matching unbind. Virtual because Node calls
    // this through a ScriptBase*, so a non-virtual override here would run the
    // base version and leave Script<N>'s typed handle unbuilt.
    virtual void bind_to(Node* host) { owner = host; }

    // --- Signals ------------------------------------------------------------
    //
    // bus, declare_signal, emit_signal and connect_signal are this script's own
    // node with node-> already filled in, so a callback does not have to spell it
    // out. connect_script_signal is the exception: it takes a target, because a
    // signal on ANOTHER node is exactly the case worth having a forwarder for.
    //
    // They are protected because a signal is only meaningful once the script is
    // bound: see owner_nonnull() below.

    // The bus, for the signals no single node owns.
    SignalBus& bus() const { return SignalBus::get_instance(); }

    // This script's node, as a Signal, so a script can declare or inspect its own
    // event without casting out to Node. Same name as Node's, same signature.
    template<typename... Args>
    Signal<Args...>& declare_signal(const std::string& signal_name) {
        return owner_nonnull()->template declare_signal<Args...>(signal_name);
    }

    // Emits on this script's own node.
    template<typename... Args>
    void emit_signal(const std::string& signal_name, Args&&... args) {
        owner_nonnull()->template emit_signal<Args...>(signal_name, std::forward<Args>(args)...);
    }

    // A reactor on THIS SCRIPT'S OWN node, owned by this script:
    //
    //     connect_signal<int>("danio", [this](int d){ vida -= d; });
    //
    // Throws if the script's own node has no such signal, which is a query you can
    // make yourself with has_signal() when the emitter may not exist yet. Use
    // connect_script_signal below for a signal on another node.
    //
    // The reactor dies with this script's node, not with the node that emits, which
    // is the right way round: the listener is the one that goes away.
    template<typename... Args, typename F>
    uint64_t connect_signal(const std::string& signal_name, F&& reactor, bool once = false) {
        Node* target = owner_nonnull();
        return target->template signal<Args...>(signal_name).connect(
            detail::make_reactor<Args...>(std::forward<F>(reactor)), owner->instance_id(), once);
    }

    // The cross-node form: a reactor that is a method of this script, connected to
    // a signal on `target`, with the arguments taken from the method's own
    // signature instead of spelled out.
    //
    //     connect_script_signal("danio", enemigo.get(), this, &MyScript::recibir_danio);
    //
    // Two reasons this exists rather than leaving you to Node's form. First the
    // owner: it is read off the script, not off `target`, so the connection dies
    // with the listener instead of with the emitter. Second the ordering: it calls
    // signal<>() rather than declare_signal(), so it THROWS when the target has no
    // such signal instead of quietly creating one nothing will ever emit - which is
    // what you want when the emitter is somebody else's node.
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
    // The one thing every signal call above assumes. Throws rather than
    // dereferencing, because the only way to reach it unbound is to call it before
    // Node has bound the script - which is a caller mistake, not a state to
    // tolerate.
    Node* owner_nonnull() const {
        if (owner == nullptr) {
            throw std::logic_error("ScriptBase: no signal call is valid before the script is bound"
                " to a node. Wire signals from on_attached or a later hook.");
        }
        return owner;
    }

    // Not a weak_ptr: the node owns this script, so a cycle is impossible. It
    // does not need invalidating either, because a script is destroyed only by
    // its own node's destructor - see the ownership note above.
    Node* owner = nullptr;
    std::string script_name = "Unnamed Script";
};

/*
    A script that knows what kind of node it belongs to.

    N is the concrete node type: Script<Node2D>, Script<NodeButton>, or plain
    Script for a node you do not care to narrow. Inside a callback, node is a
    ready-to-use N*, so reaching a property needs no cast and no call to
    get_owner_as<T>().

    Binding a Script<N> to a node that is not an N is a programming error and is
    reported as such: bind_to throws std::invalid_argument rather than handing you
    a typed pointer that points at the wrong object. That check is a runtime
    dynamic_cast, not a compile-time one, because attach_script<T>() cannot know the
    type of the node calling it.
*/
template<typename N = Node>
class Script : public ScriptBase {
public:
    // Publishes N so that Node::attach_script<T>() can read it off a script type:
    // it asserts N is a Node type, and that the script came from this template
    // rather than from ScriptBase directly. See has_node_type in Node.hpp.
    using node_type = N;

protected:
    // The owner, as its concrete type. Non-null for as long as this script is
    // bound to a node, which is the whole of every callback's duration: the node
    // owns us, so it cannot be destroyed underneath a running method.
    N* node = nullptr;

    void bind_to(Node* host) override {
        // Checked before touching any state, so a rejected bind leaves the
        // script exactly as unbound as it was.
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