#ifndef GUAYABA_NODES_NODE_HPP
#define GUAYABA_NODES_NODE_HPP

#include <cstdint>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <guayaba/Core/NodeRegistry.hpp>
#include <guayaba/Core/NodeType.hpp>
#include <guayaba/Core/Signal.hpp>

namespace guayaba {

class SceneManager;
class ConsoleRenderer;
class ScriptBase;

template<typename N>
class NodeRef;

template<typename N, typename S>
class ScriptRef;

namespace detail {

// Detects the marker Script<N> carries, so that attach_script<T>() can reject a T
// written against ScriptBase directly with a readable message instead of a
// "no type named node_type" cascade. The body that would need node_type is only
// instantiated once this says yes, so the failure lands on the static_assert.
template<typename T, typename = void>
struct has_node_type : std::false_type {};

template<typename T>
struct has_node_type<T, std::void_t<typename T::node_type>> : std::true_type {};

} // namespace detail

/*
    OWNERSHIP
    The tree owns its children outright (unique_ptr) and holds its parent as a
    plain pointer. There is no refcounting anywhere in the tree, which means a
    reference cycle is not merely discouraged, it cannot be written.

    The rule that follows: a node belongs to whoever created it right up to the
    moment it is handed over, and to the tree from then on.

        auto root = std::make_unique<Node>("root");      // you own it
        NodeRef<NodeUI> title = root->add_child<NodeUI>(...); // tree owns it now
        SceneManager::get_instance().change_scene(std::move(root));

    Nothing that names a node hands out a raw pointer. Every reference is a
    NodeRef<T>, which holds an id rather than an address and so answers nullptr
    once the node is gone instead of dangling. To reach the node, call get() and
    check it; see NodeRef.hpp.

    Children are named three ways, and none of them needs the tree walked to be
    safe: child_index() for position, get_path()/is_ancestor_of() for
    relationship, and NodeRef for identity across a free.

    SIGNALS
    A node also carries its own events, so two parts of a tree can talk without
    either holding a handle to the other:

        player->emit_signal("danio", 7);

    A reactor connected from a node is dropped when that node is destroyed, which
    is what makes it safe to connect from a Script without bookkeeping. See
    Core/Signal.hpp for what that rests on.

    Non-copyable: a copy would duplicate the instance id, leaving one slot
    pointing at two nodes, and the first destructor to run would invalidate the
    survivor's handles.
*/
class Node {
    friend class SceneManager;

    // Reads the script without downcasting. ScriptRef needs the raw pointer to
    // static_cast it, which it can only justify because of script_sealed below.
    template<typename, typename>
    friend class ScriptRef;

protected:
    std::vector<std::unique_ptr<Node>> children;
    Node* parent = nullptr;
    std::string name;
    std::unique_ptr<ScriptBase> script;

    mutable bool in_tree = false;
    bool queued_for_deletion = false;

    // Set on the first successful attach and never cleared. A script lives
    // exactly as long as its node from that point on, which is what lets a
    // ScriptRef re-derive the script from the node's id without holding a
    // pointer that could dangle. See Core/ScriptRef.hpp.
    bool script_sealed = false;

    // This node's own events, by name. Heterogeneous by necessity - "danio" is
    // void(int) and "murio" is void() and both belong to the same node - so the
    // table erases the arguments and the member templates below restore them.
    // See Core/Signal.hpp.
    SignalTable signals;

    virtual void enter_tree();
    virtual void exit_tree();

    // Iterates children in a way that tolerates the tree being mutated from
    // inside a callback: adding is fine, and a child that removes itself is
    // re-stepped rather than skipped. Reads only the vector's storage, never the
    // child, which may already be destroyed.
    template<typename Fn>
    void for_each_child(Fn&& fn);

private:
    uint64_t instance_id_ = detail::NodeRegistry::INVALID_ID;

    // This node's slot in its parent's children, or SIZE_MAX when it has no
    // parent. Maintained by the parent: only remove_child() and move_child()
    // shift anything, since add_child() appends. Read it through
    // child_index(), and read it fresh at the point of use - storing the number
    // across a mutation is what makes it mean something else by then.
    size_t child_index_ = SIZE_MAX;

    // The script as a ScriptBase*, for ScriptRef to narrow itself. Deliberately
    // private: narrowing to S is only sound because the slot is sealed.
    ScriptBase* script_raw() const { return script.get(); }

    // Hands this node's ownership to the caller: unparents, leaves the tree,
    // fires on_exit_tree. Used by remove_child and by the deletion path.
    std::unique_ptr<Node> detach_ownership();

    std::string make_child_name_unique(const std::string& wanted) const;

    // Restores child_index_ after children have shifted. Only remove_child() and
    // move_child() need this: add_child() appends, so nothing moves.
    void reindex_from(size_t start);

    Node* resolve_path(const std::string& path) const;

    // Two different mistakes, two different messages: the name was never declared
    // on this node, or it is declared with other arguments and asking for it as
    // Args... would produce a second signal under the same name.
    template<typename... Args>
    std::logic_error signal_shape_error(const std::string& signal_name) const {
        const char* existing = signals.signature_of(signal_name);
        if (existing != nullptr) {
            return std::logic_error("Node::signal: '" + name + "' declares \"" + signal_name
                + "\" as " + existing + ", not the requested signature.");
        }
        return std::logic_error("Node::signal: '" + name + "' declares no signal named \""
            + signal_name + "\". connect_signal and emit_signal declare one on demand.");
    }

public:
    explicit Node(std::string node_name = "Unnamed Node");

    virtual ~Node();

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    Node(Node&&) = delete;
    Node& operator=(Node&&) = delete;

    // --- Building -----------------------------------------------------------

    // Constructs in place and hands the ownership to this node. A duplicate
    // sibling name becomes "name@2", so paths stay unambiguous.
    // The definition lives in Core/NodeRef.hpp: returning a NodeRef by value
    // needs the complete type, and that header includes this one.
    template<typename T, typename... Args>
    NodeRef<T> add_child(Args&&... args);

    // The ownership hand-off. A node that already has a parent is rejected: take
    // it back with remove_child(index) and add it to the new parent yourself.
    // Adding a child to its own current parent appends it, which changes draw
    // order; there is no same-parent no-op here.
    NodeRef<Node> add_child(std::unique_ptr<Node> child);

    // Detaches and hands ownership back. Never destroys, so unlike Godot's
    // remove_child() it cannot produce an orphan you forgot to free - if you
    // drop the returned unique_ptr, the subtree goes with it.
    //
    // Takes the child's slot, from child_index(). Removing a node that is not a
    // child of this one is a programming error and throws, rather than quietly
    // returning nullptr.
    std::unique_ptr<Node> remove_child(size_t index);

    // The same hand-off, asked from the node's own side. Useful when you are
    // holding a NodeRef to the child rather than to its parent, since it saves
    // walking up to the parent and back down to the index:
    //
    //     std::unique_ptr<Node> mio = ref.get()->steal();
    //
    // Equivalent to parent->remove_child(child_index()). Never destroys, so
    // dropping the result takes the subtree with it.
    //
    // Throws when the node has no parent: a root belongs to whoever created it,
    // or to the SceneManager, and this does not reach them. Use take_scene() or
    // change_scene(nullptr) for a scene root.
    std::unique_ptr<Node> steal();

    // --- Destruction --------------------------------------------------------

    // REQUIRES THE FRAME LOOP. Both of these only arrange destruction; the
    // SceneManager performs it, at the end of the frame. Calling them outside a
    // running loop leaves the node pending and undestroyed.

    // Unparents now, so the node leaves the tree and stops drawing immediately.
    // Destroyed at the next drain, which is what makes this safe to call from
    // inside a callback.
    void free();

    // Stays parented: keeps updating and drawing until the drain, matching Godot,
    // where a queued node still renders its final frame.
    void queue_free();

    bool is_queued_for_deletion() const { return queued_for_deletion; }

    // --- Tree navigation ----------------------------------------------------

    NodeRef<Node> get_parent() const;

    // Empty when no ancestor has type T.
    template<typename T>
    NodeRef<T> get_parent_of_type() const;

    bool is_root() const { return parent == nullptr; }

    // True when the given path names this node or something below it. Used to
    // reject an add_child() that would close a loop.
    //
    // Compares path strings, so it is scoped to one tree: two unrelated trees
    // can each contain "/root/a", and this would report a relationship that
    // does not exist. That direction is safe - add_child() turns a false
    // positive into a refused add rather than an actual cycle.
    //
    //     child->is_ancestor_of(parent->get_path())
    bool is_ancestor_of(const std::string& path) const;

    size_t get_child_count() const { return children.size(); }

    // This node's slot in its parent, or SIZE_MAX when it has no parent. Read
    // it where you use it; a stored number stops naming this child once a
    // sibling before it is added or removed.
    size_t child_index() const { return child_index_; }

    // Throws if the index is out of bounds, or if the child there is not a T.
    template<typename T = Node>
    NodeRef<T> get_child(size_t index) const;

    bool has_child(const std::string& child_name) const;

    // Draw order follows child order, so this is z-order. `to` is the final
    // position, after the move, not the one the shifted-out node ends up in.
    void move_child(size_t from, size_t to);

    // "a/b", "/a/b", "." and ".." are all understood, resolved against the tree
    // rather than stored, so a NodeRef plus a path survives being moved.
    // Throws when there is no node there.
    template<typename T = Node>
    NodeRef<T> get_node(const std::string& path) const;

    // Empty rather than throwing when the path names nothing.
    template<typename T = Node>
    NodeRef<T> get_node_or_null(const std::string& path) const;

    // Pre-order, first match wins. Empty when there is no such child.
    NodeRef<Node> find_child(const std::string& wanted_name, bool recursive = true) const;

    // Absolute, from the topmost ancestor. Names cannot contain '/', so a path
    // is unambiguous within one tree.
    std::string get_path() const;

    // The path from this node to the given path, or "." when they are the same
    // node. Takes a path rather than a node, so it also answers for a node that
    // has already been freed.
    std::string get_path_to(const std::string& path) const;

    // --- Identity -----------------------------------------------------------

    // Where to put a child named this. Called by add_child, so a name that
    // cannot be addressed is refused at construction rather than producing a
    // path that resolves to something else.
    const std::string& get_name() const { return name; }

    uint64_t instance_id() const { return instance_id_; }

    // Resolves an id captured earlier. Returns nullptr once that node is gone.
    static Node* resolve_instance_id(uint64_t id);

    // Live nodes constructed but not yet destroyed. Non-zero after tearing the
    // scene down means something leaked.
    static size_t live_node_count();

    virtual NodeType get_type() const;

    bool is_in_tree() const { return in_tree; }

    // --- Behaviour ----------------------------------------------------------

    // An override that fails to delegate to the base implementation silently
    // disables the attached script for that node type. Every override of update
    // and draw MUST end with Node::update(...) / Node::draw(...).
    virtual void update(double delta_time);

    // Builds a script in place, attaches it, and hands back a safe reference to
    // it. The node owns the script from here on:
    //
    //     ScriptRef<Node2D, Blinker> b = node->attach_script<Blinker>();
    //     b.script()->period = 0.2;
    //
    // Attach is build-time only: once a node has a script it keeps that script
    // until the node is destroyed, so a second attach throws rather than
    // invalidating whatever held the first one. Detaching does not exist.
    //
    // The definition lives in Core/ScriptRef.hpp because returning ScriptRef by
    // value needs the complete type, and that header includes this one.
    template<typename S, typename... Args>
    ScriptRef<typename S::node_type, S> attach_script(Args&&... args);

    // Attaches an already-built script. Returns void because with no S on the
    // caller's side there is no type to hand back a ScriptRef as.
    void attach_script(std::unique_ptr<ScriptBase> new_script);

    bool has_script() const { return static_cast<bool>(script); }

    // Asks whether this node currently holds a script of type S. A query, so a
    // mismatch yields an empty ScriptRef rather than throwing; use
    // attach_script<S>() when you want the node's script to BE an S.
    //
    // S must derive from Script<N>, for the same reason attach_script<S>()
    // requires it: naming node_type is what lets the answer stay cast-free.
    // That does rule out asking about a plain ScriptBase.
    template<typename S>
    ScriptRef<typename S::node_type, S> get_script_as() const;

    // --- Signals ------------------------------------------------------------
    //
    // A node's own events. emit_signal calls every reactor connected to that name
    // on this node, in the order they were connected:
    //
    //     player->emit_signal("danio", 7);
    //     barra->connect_signal<int>("danio", [barra_weak](int d){ refresh(d); });
    //
    // Nothing here needs a handle to the other node if you wire it from the same
    // builder function, because connect_signal's owner is the node doing the
    // connecting, not the node that will later emit.
    //
    // A reactor connected here is dropped automatically when THIS node is
    // destroyed, so a script may connect freely without recording ids or
    // disconnecting on the way out. See Core/Signal.hpp for why that holds.

    // Declares this node's signal, typed by Args. Optional: connect_signal and
    // emit_signal both declare on demand, and re-declaring the same shape hands
    // back the same signal. Declaring it yourself is what makes the signature
    // discoverable before anything is emitted, which is why NodeButton declares
    // its own "clicked".
    template<typename... Args>
    Signal<Args...>& declare_signal(const std::string& signal_name) {
        return signals.declare<Args...>(signal_name, "Node '" + name + "'");
    }

    // The signal itself, for counting reactors or disconnecting by id. Throws if
    // the name is not declared with this signature, because "that one exists but
    // takes different arguments" is a programming error and not a query result.
    template<typename... Args>
    Signal<Args...>& signal(const std::string& signal_name) const {
        Signal<Args...>* found = signals.find<Args...>(signal_name);
        if (found == nullptr) throw signal_shape_error<Args...>(signal_name);
        return *found;
    }

    // A query, so a mismatch and an absent signal are both false rather than a
    // throw. This is what an emitter checks to avoid declaring a signal nobody
    // listens to.
    template<typename... Args>
    bool has_signal(const std::string& signal_name) const {
        return signals.find<Args...>(signal_name) != nullptr;
    }

    // Declares on demand, then adds a reactor. Returns the id that disconnects
    // it, which most callers never need to keep: the connection dies with this
    // node either way. once drops the reactor after its first call.
    //
    // The arguments have to be spelled here - connect_signal<int>("danio", ...) -
    // because they cannot be read off a lambda. Inside a Script, prefer
    // connect_script_signal below or ScriptBase::connect_signal, which take them
    // from a method's own signature instead.
    template<typename... Args, typename F>
    uint64_t connect_signal(const std::string& signal_name, F&& reactor, bool once = false) {
        return declare_signal<Args...>(signal_name).connect(
            detail::make_reactor<Args...>(std::forward<F>(reactor)), instance_id_, once);
    }

    // The form to use from inside a Script when the reactor is a method of that
    // script: the arguments come from the method's own signature, so nothing is
    // spelled twice, and there is no lambda capturing `this` to get wrong.
    //
    //     connect_signal("danio", this, &MyScript::recibir_danio);
    //     void MyScript::recibir_danio(int d) { vida -= d; }
    template<typename R, typename C, typename... Args>
    uint64_t connect_script_signal(const std::string& signal_name,
                                   C* reactor,
                                   R (C::*member)(Args...),
                                   bool once = false) {
        return declare_signal<std::decay_t<Args>...>(signal_name).connect(
            [reactor, member](std::decay_t<Args>... args) {
                (reactor->*member)(std::forward<decltype(args)>(args)...);
            },
            instance_id_, once);
    }

    // Calls every reactor currently connected to that name. Args are deduced and
    // decayed, so emit_signal("danio", d) finds the void(int) signal whether d is
    // an int, an unsigned or a size_t copy of one. The arguments are forwarded to
    // the reactors unchanged; the reactor list itself is snapshotted, so a reactor
    // may connect, disconnect or free nodes while this runs.
    template<typename... Args>
    void emit_signal(const std::string& signal_name, Args&&... args) {
        declare_signal<std::decay_t<Args>...>(signal_name).emit(std::forward<Args>(args)...);
    }

    // A connection id is what connect_signal returned. False when it is not live
    // any more, which includes the case where its owner node has been destroyed.
    bool is_connected(uint64_t connection_id) const { return signals.is_connected(connection_id); }

    bool disconnect_signal(uint64_t connection_id) { return signals.disconnect(connection_id); }

    size_t get_signal_count() const { return signals.size(); }

    // Drops every connection on this node's signals. Nothing in the engine calls
    // it: the connections die with the node, and a live node keeps the ones it was
    // given. It exists for the scene teardown case, where you want them gone
    // before anything else starts emitting into the next scene.
    void clear_signals() { signals.clear(); }

    virtual void draw(ConsoleRenderer& renderer);

    virtual void invalidate_cache_recursively();
};

} // namespace guayaba

#endif // GUAYABA_NODES_NODE_HPP