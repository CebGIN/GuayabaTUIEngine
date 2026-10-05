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

    OWNERSHIP
    The node owns its script, and that is what makes the back reference cheap:
    while any method of yours is running, the node is guaranteed alive, because it
    owns the object you are executing on. So owner is a plain Node* - no lock, no
    reference count, no validity check per frame.

        class Blinker : public Script {
        public:
            double period = 0.5;
            double elapsed = 0.0;
            void on_process(double dt) override {
                elapsed += dt;
                if (elapsed >= period) { elapsed = 0.0; // invert the colours }
            }
        };

        Blinker* blink = node->add_script<Blinker>();
        blink->period = 0.2;   // your own fields, through your own handle

    SUBCLASSING SCRIPT IS THE NORMAL AND CHEAP ROUTE. Declare your properties
    as plain typed fields and reach them through the handle add_script<T>()
    returned; that is the short path and needs no cast.

    get_owner() yields a Node*. If you need the concrete type, get_owner_as<T>()
    casts for you, static_assert included.
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

    // Null once the script is detached. Safe without a check for as long as this
    // method is running, because the node owns us.
    Node* get_owner() const { return owner; }

    template<typename T>
    T* get_owner_as() const {
        static_assert(std::is_base_of<Node, T>::value, "T must be a Node type.");
        return dynamic_cast<T*>(owner);
    }

protected:
    friend class Node;

    // Only Node binds and unbinds scripts.
    void bind_to(Node* host) { owner = host; }
    void unbind() { owner = nullptr; }

private:
    // Not a weak_ptr: the node owns this script, so a cycle is impossible and the
    // pointer needs no invalidation.
    Node* owner = nullptr;
    std::string script_name = "Unnamed Script";
};

} // namespace guayaba

#endif // GUAYABA_CORE_SCRIPT_HPP