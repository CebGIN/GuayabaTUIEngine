#ifndef GUAYABA_CORE_SCRIPT_HPP
#define GUAYABA_CORE_SCRIPT_HPP

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include <guayaba/Nodes/Node.hpp>

namespace guayaba {

class ConsoleRenderer;

/*
    Behaviour that attaches to an existing Node without deriving from that
    node's C++ class. The idea is Godot's Script: bolt logic and your own state
    onto a prefab you did not write, without reimplementing its rendering.

    Two lifecycles live here, and they are deliberately not mixed:

      - The NODE's lifecycle -> on_enter_tree / on_exit_tree. Fired only when
        the node actually joins or leaves the scene tree.
      - The SCRIPT's lifecycle -> on_attached / on_detached. Fired only when the
        script is bound to, or unbound from, a node.

    A script always assumes it is inside the tree, so attaching one to a node
    that is already live will never deliver on_enter_tree. Ask for it yourself

    SUBCLASSING SCRIPT IS THE NORMAL AND CHEAP ROUTE. Declare your properties
    as plain typed fields there and reach them through your own reference, which
    is the short path and needs no cast:

        auto blink = std::make_shared<Blinker>();
        node->attach_script(blink);
        blink->period = 0.2f;   // neither node-> nor dynamic_cast

    get_owner() yields a shared_ptr<Node>. If you need the concrete type,
    get_owner_as<T>() casts for you, static_assert included.
*/
class Script {
public:
    virtual ~Script() = default;

    // Script lifecycle.
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

    // Empty once the node is gone. The back reference is weak on purpose:
    // a weak Node -> Script -> Node link cannot form a reference cycle.
    std::shared_ptr<Node> get_owner() const { return owner.lock(); }

    template<typename T>
    std::shared_ptr<T> get_owner_as() const {
        static_assert(std::is_base_of<Node, T>::value, "T must be a Node type.");
        return std::dynamic_pointer_cast<T>(owner.lock());
    }

protected:
    friend class Node;

    // Only Node binds and unbinds scripts.
    void bind_to(const std::shared_ptr<Node>& host) { owner = host; }
    void unbind() { owner.reset(); }

private:
    std::weak_ptr<Node> owner;
    std::string script_name = "Unnamed Script";
};

} // namespace guayaba

#endif // GUAYABA_CORE_SCRIPT_HPP