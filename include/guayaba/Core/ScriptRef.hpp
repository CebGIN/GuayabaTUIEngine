#ifndef GUAYABA_CORE_SCRIPTREF_HPP
#define GUAYABA_CORE_SCRIPTREF_HPP

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <typeinfo>
#include <utility>

#include <guayaba/Core/NodeRef.hpp>
#include <guayaba/Core/Script.hpp>

namespace guayaba {

/*
    Dangling-safe handle to a script, for scripts you did not attach yourself.

    A raw S* from attach_script<S>() is fine inside the subtree that owns it, but
    a script on another node can be freed while you still hold it. ScriptRef
    stores the owner's id and re-derives the script on every access, the same way
    NodeRef handles a Node*:

        class Watcher : public Script<Node2D> {
        public:
            explicit Watcher(ScriptRef<Node2D, Blinker> b) : blink(b) {}
            void on_process(double) override {
                if (auto* b = blink.script()) b->period = 0.5;
            }
            ScriptRef<Node2D, Blinker> blink;
        };

    script() resolves the id and static_casts the node's script to S. That cast
    is sound because the node holds the first script it was given until it dies
    (Node::script_sealed) and both constructors below only accept a script the
    node actually holds. It does not keep the script alive, and a handle to an
    orphan is still live.
*/
template<typename N, typename S>
class ScriptRef {
    static_assert(std::is_base_of<Node, N>::value, "N must be a Node type.");
    static_assert(detail::has_node_type<S>::value, "S must derive from Script<N>, not ScriptBase: see Core/Script.hpp.");

    // Checks that N and the node type S was declared for agree.
    static_assert(std::is_base_of<N, typename S::node_type>::value,
        "N must be the node type S was declared for, or a base of it.");

public:
    ScriptRef() = default;

    // Discovers the S the owner holds. Throws if it holds none.
    explicit ScriptRef(N* owner) : ScriptRef(owner, owner != nullptr ? owner->template get_script_as<S>() : nullptr) {}

    // Throws if `owner` does not hold exactly this script.
    ScriptRef(N* owner, S* script) {
        static_assert(!std::is_abstract<S>::value, "S must be concrete.");

        if (owner == nullptr) {
            throw std::invalid_argument(std::string("ScriptRef<") + typeid(N).name()
                + ", " + typeid(S).name() + ">: null owner node.");
        }
        if (script == nullptr) {
            throw std::invalid_argument(std::string("ScriptRef<") + typeid(N).name()
                + ", " + typeid(S).name() + ">: null script.");
        }

        // Compares against the sealed slot directly; get_script_as<S>() would
        // build a ScriptRef and recurse back here.
        S* held = dynamic_cast<S*>(owner->script_raw());
        if (held != script) {
            throw std::invalid_argument(std::string("ScriptRef<") + typeid(N).name()
                + ", " + typeid(S).name() + ">: that script is not attached to that node.");
        }

        id_ = owner->instance_id();
    }

    // Null once the owner is destroyed. Safe because the seal makes the cast
    // sound; the id is the only check.
    S* script() const {
        Node* node = detail::NodeRegistry::instance().resolve(id_);
        return node != nullptr ? static_cast<S*>(node->script_raw()) : nullptr;
    }

    // The owning node, or nullptr once it is gone.
    N* node() const {
        Node* node = detail::NodeRegistry::instance().resolve(id_);
        return node != nullptr ? static_cast<N*>(node) : nullptr;
    }

    // Builds an empty or explicit handle from a raw id. Not meant to be called by
    // hand.
    ScriptRef(uint64_t raw_id, int) : id_(raw_id) {}

    bool is_valid() const { return script() != nullptr; }

    explicit operator bool() const { return is_valid(); }

    S* operator->() const {
        S* s = script();
        assert(s != nullptr && "ScriptRef::operator-> on a dead reference.");
        return s;
    }

    // The owner's id, which is all this handle stores.
    uint64_t id() const { return id_; }

private:
    uint64_t id_ = detail::NodeRegistry::INVALID_ID;
};

/*
    Declared in Nodes/Node.hpp and defined here because returning a ScriptRef by
    value needs the complete type, and this header includes Node.hpp.
*/
template<typename S, typename... Args>
ScriptRef<typename S::node_type, S> Node::attach_script(Args&&... args) {
    static_assert(std::is_base_of<ScriptBase, S>::value, "S must be a ScriptBase type.");
    static_assert(!std::is_abstract<S>::value, "S must be concrete: a hook you cannot override is a hook nothing dispatches to.");
    static_assert(detail::has_node_type<S>::value, "S must derive from Script<N>, not ScriptBase: see Core/Script.hpp.");
    static_assert(std::is_base_of<Node, typename S::node_type>::value, "A Script must name a Node type as its node_type.");

    std::unique_ptr<ScriptBase> owned(new S(std::forward<Args>(args)...));
    S* handle = static_cast<S*>(owned.get());
    attach_script(std::move(owned));
    return ScriptRef<typename S::node_type, S>(static_cast<typename S::node_type*>(this), handle);
}

/*
    Declared in Nodes/Node.hpp and defined here because returning a ScriptRef by
    value needs the complete type, and this header includes Node.hpp.
*/
template<typename S>
ScriptRef<typename S::node_type, S> Node::get_script_as() const {
    static_assert(std::is_base_of<ScriptBase, S>::value, "S must be a ScriptBase type.");
    static_assert(detail::has_node_type<S>::value,
        "S must derive from Script<N>, not ScriptBase: see Core/Script.hpp.");

    S* typed = dynamic_cast<S*>(script.get());
    // A miss returns an empty ref with INVALID_ID: this node's id would resolve
    // and make script() narrow a non-S to S*.
    if (typed == nullptr) {
        return ScriptRef<typename S::node_type, S>(detail::NodeRegistry::INVALID_ID, 0);
    }
    // The owner is the S::node_type ScriptRef expects: attach_script<S>() only
    // binds to a node that dynamic_casts to S::node_type, and the slot is sealed.
    return ScriptRef<typename S::node_type, S>(
        static_cast<typename S::node_type*>(const_cast<Node*>(this)), typed);
}

} // namespace guayaba

#endif // GUAYABA_CORE_SCRIPTREF_HPP
