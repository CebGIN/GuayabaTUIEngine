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

// Detects the marker Script<N> carries, so that attach_script<T>() can reject a T
// written against ScriptBase directly with a readable message instead of a
// "no type named node_type" cascade. The body that would need node_type is only
// instantiated once this says yes, so the failure lands on the static_assert.
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

    template<typename Fn>
    void for_each_child(Fn&& fn);

private:
    uint64_t instance_id_ = detail::NodeRegistry::INVALID_ID;

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

    // --- Behaviour ----------------------------------------------------------

    virtual void update(double delta_time);

    // --- Layout -------------------------------------------------------------
    // Once per frame, before draw(). The root is handed the console rect and
    // each node passes down whatever space its children should resolve against,
    // so a Control always measures against its nearest laid-out ancestor rather
    // than having to walk the tree itself.
    //
    // The base implementation forwards parent_rect unchanged: a Control nested
    // under plain Node2D wrappers still resolves against the last real
    // Control above, not against a rect those wrappers never computed.
    //
    // This is "lay my children out against the rect I'm given", nothing more.
    // A subclass that needs to resolve itself against that rect first, and then
    // hand its own rect down instead, calls back into this with its resolved
    // rect - see Control::update_layout, which is why there is only one virtual
    // here and no separate propagate step.
    virtual void update_layout(const Rect& parent_rect);

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

    // clip is the region drawing is currently confined to, in buffer
    // coordinates, and is passed down to every child. A node passes it through
    // unchanged; a Control that clips narrows it by its own rect first (see
    // Control::draw).
    //
    // Scripts have no drawing hook: how a node looks is decided by the node
    // itself, so overriding draw() is the only way to draw.
    virtual void draw(ConsoleRenderer& renderer, const Rect& clip);

    virtual void invalidate_cache_recursively();

};

} // namespace guayaba

#endif // GUAYABA_NODES_NODE_HPP