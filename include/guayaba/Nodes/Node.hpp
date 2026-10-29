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

#include <guayaba/Core/InputEvent.hpp>
#include <guayaba/Core/NodeRegistry.hpp>
#include <guayaba/Core/Rect.hpp>
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

// Detects the marker Script<N> carries, so attach_script<T>() can reject a T
// written against ScriptBase directly with a static_assert message.
template<typename T, typename = void>
struct has_node_type : std::false_type {};

template<typename T>
struct has_node_type<T, std::void_t<typename T::node_type>> : std::true_type {};

} // namespace detail

class Node {
    friend class SceneManager;

    template<typename, typename>
    friend class ScriptRef;

protected:
    std::vector<std::unique_ptr<Node>> children;
    Node* parent = nullptr;
    std::string name;
    std::unique_ptr<ScriptBase> script;

    mutable bool in_tree = false;
    bool queued_for_deletion = false;

    bool script_sealed = false;

    SignalTable signals;

    virtual void enter_tree();
    virtual void exit_tree();

    /*
        Frame phases. The public update/update_layout/route_input/
        route_unhandled_input/draw are non-virtual wrappers that check visibility
        first; these hooks are what a subclass overrides. From an override, call
        the base hook (Node::on_update, Control::on_draw, ...), never the wrapper:
        the wrapper would check visibility again and dispatch back here.
    */
    virtual void on_update(double delta_time);
    virtual void on_update_layout(const Rect& parent_rect);
    virtual void on_route_input(std::vector<InputEvent>& events);
    virtual void on_route_unhandled_input(const InputEvent& event);
    virtual void on_draw(ConsoleRenderer& renderer, const Rect& clip);

    // visible && every ancestor visible. What the wrappers test.
    bool is_active_in_tree() const { return is_visible_in_tree(); }

    template<typename Fn>
    void for_each_child(Fn&& fn);

private:
    uint64_t instance_id_ = detail::NodeRegistry::INVALID_ID;

    bool visible_ = true;

    size_t child_index_ = SIZE_MAX;

    ScriptBase* script_raw() const { return script.get(); }

    std::unique_ptr<Node> detach_ownership();

    std::string make_child_name_unique(const std::string& wanted) const;

    void reindex_from(size_t start);

    Node* resolve_path(const std::string& path) const;

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

    template<typename T, typename... Args>
    NodeRef<T> add_child(Args&&... args);

    NodeRef<Node> add_child(std::unique_ptr<Node>&& child);

    std::unique_ptr<Node> remove_child(size_t index);

    std::unique_ptr<Node> steal();

    // --- Destruction --------------------------------------------------------

    void free();

    void queue_free();

    bool is_queued_for_deletion() const { return queued_for_deletion; }

    // --- Tree navigation ----------------------------------------------------

    NodeRef<Node> get_parent() const;

    // Empty when no ancestor has type T.
    template<typename T>
    NodeRef<T> get_parent_of_type() const;

    bool is_root() const { return parent == nullptr; }

    bool is_ancestor_of(const std::string& path) const;

    size_t get_child_count() const { return children.size(); }

    size_t child_index() const { return child_index_; }

    template<typename T = Node>
    NodeRef<T> get_child(size_t index) const;

    bool has_child(const std::string& child_name) const;

    void move_child(size_t from, size_t to);

    template<typename T = Node>
    NodeRef<T> get_node(const std::string& path) const;

    template<typename T = Node>
    NodeRef<T> get_node_or_null(const std::string& path) const;

    NodeRef<Node> find_child(const std::string& wanted_name, bool recursive = true) const;

    std::string get_path() const;

    std::string get_path_to(const std::string& path) const;

    // --- Identity -----------------------------------------------------------

    const std::string& get_name() const { return name; }

    uint64_t instance_id() const { return instance_id_; }

    static Node* resolve_instance_id(uint64_t id);

    static size_t live_node_count();

    bool is_in_tree() const { return in_tree; }

    // --- Visibility ---------------------------------------------------------

    /*
        Hides or shows this subtree. A hidden node keeps its place in the tree -
        name, path, signals, script and ownership - but the frame loop skips it:
        no update, layout, input or draw. Containers do not reserve room for it
        and FocusManager does not walk it.

        Hiding releases the focus only when the focused control is inside this
        subtree, and drops the cached rects so stale geometry is not hit-tested.
        Showing invalidates them again; the next layout pass resolves the rects.
    */
    void set_visible(bool visible);

    // This node's own flag, ignoring its ancestors.
    bool is_visible() const { return visible_; }

    // False when this node or any ancestor is hidden. A detached node counts as
    // visible: it has no ancestors that could hide it.
    bool is_visible_in_tree() const;

    // True when `node` is this node or one of its descendants.
    bool is_ancestor_of(const Node* node) const;

    // --- Behaviour ----------------------------------------------------------

    /*
        The five frame phases below are non-virtual on purpose: the visibility
        check lives in these wrappers, so no subclass can forget it. Override the
        matching on_* hook instead.
    */
    void update(double delta_time);

    // --- Layout -------------------------------------------------------------
    // Once per frame, before draw(). Each node passes down the space its children
    // resolve against, so a Control measures against its nearest laid-out ancestor
    // without walking the tree. The base forwards parent_rect unchanged; a subclass
    // that resolves itself first re-enters this hook with its own rect (see
    // Control::on_update_layout).
    void update_layout(const Rect& parent_rect);

    /*
        Hands this subtree the frame's input events. Children are visited in reverse
        (topmost first), then what is left goes to this node's script via on_input.
        Consuming an event erases it from the vector, so it cannot reach another
        control. The frame loop calls this after update_layout, so hit-tested rects
        are the ones about to be drawn.
    */
    void route_input(std::vector<InputEvent>& events);

    /*
        Delivers one unconsumed event to this subtree in forward order, calling this
        node's script on_unhandled_input. Not consumable.
    */
    void route_unhandled_input(const InputEvent& event);

    template<typename S, typename... Args>
    ScriptRef<typename S::node_type, S> attach_script(Args&&... args);

    void attach_script(std::unique_ptr<ScriptBase> new_script);

    bool has_script() const { return static_cast<bool>(script); }

    template<typename S>
    ScriptRef<typename S::node_type, S> get_script_as() const;

    // --- Signals ------------------------------------------------------------

    template<typename... Args>
    Signal<Args...>& declare_signal(const std::string& signal_name) {
        return signals.declare<Args...>(signal_name, "Node '" + name + "'");
    }

    template<typename... Args>
    Signal<Args...>& signal(const std::string& signal_name) const {
        Signal<Args...>* found = signals.find<Args...>(signal_name);
        if (found == nullptr) throw signal_shape_error<Args...>(signal_name);
        return *found;
    }


    template<typename... Args>
    bool has_signal(const std::string& signal_name) const {
        return signals.find<Args...>(signal_name) != nullptr;
    }

    template<typename... Args, typename F>
    uint64_t connect_signal(const std::string& signal_name, F&& reactor,
                            bool once = false, Node* owner = nullptr) {
        return declare_signal<Args...>(signal_name).connect(
            detail::make_reactor<Args...>(std::forward<F>(reactor)),
            owner != nullptr ? detail::node_id_of(owner) : instance_id_, once);
    }

    template<typename R, typename C, typename... Args>
    uint64_t connect_script_signal(const std::string& signal_name,
                                   C* reactor,
                                   R (C::*member)(Args...),
                                   bool once = false) {
        if (reactor == nullptr) {
            throw std::invalid_argument("Node::connect_script_signal: null reactor.");
        }
        return declare_signal<std::decay_t<Args>...>(signal_name).connect(
            [reactor, member](std::decay_t<Args>... args) {
                (reactor->*member)(std::forward<decltype(args)>(args)...);
            },
            detail::reactor_owner<C>::of(reactor, instance_id_), once);
    }

    template<typename... Args>
    void emit_signal(const std::string& signal_name, Args&&... args) {
        declare_signal<std::decay_t<Args>...>(signal_name).emit(std::forward<Args>(args)...);
    }

    bool is_connected(uint64_t connection_id) const { return signals.is_connected(connection_id); }

    bool disconnect_signal(uint64_t connection_id) { return signals.disconnect(connection_id); }

    size_t get_signal_count() const { return signals.size(); }

    void clear_signals() { signals.clear(); }

    // --- virtual transversals ------------------------------------------------------------

    // clip is the region drawing is confined to, in buffer coordinates, passed down
    // to every child. A Control that clips narrows it by its own rect first (see
    // Control::on_draw). Scripts have no drawing hook; overriding on_draw() is the
    // only way to draw.
    void draw(ConsoleRenderer& renderer, const Rect& clip);

    virtual void invalidate_cache_recursively();

};

} // namespace guayaba

#endif // GUAYABA_NODES_NODE_HPP