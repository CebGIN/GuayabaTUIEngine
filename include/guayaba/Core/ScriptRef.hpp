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

    A raw S* from attach_script<S>() is fine inside the subtree that owns it.
    It is not fine anywhere else: a script on another node can be freed while
    you still hold it, and nothing would tell you. ScriptRef is the answer for
    that case, the same way NodeRef is the answer for a Node*.

        class Watcher : public Script<Node2D> {
        public:
            // attach_script forwards its arguments to S, so the target arrives
            // the way any other constructor argument would.
            explicit Watcher(ScriptRef<Node2D, Blinker> b) : blink(b) {}
            void on_process(double) override {
                if (auto* b = blink.script()) b->period = 0.5;
            }
            ScriptRef<Node2D, Blinker> blink;
        };

    WHY IT HOLDS NO SCRIPT POINTER
    A script's identity is fully determined by its owner and its type, because
    the first script a node gets is the one it keeps until it dies. So this
    stores the owner's id and re-derives the script on every access:

        script()  ->  resolve the id, then static_cast the node's script to S

    That is 8 bytes and no pointer, which means it inherits NodeRef's two
    guarantees for free. It cannot dangle, and it cannot silently alias a
    different script constructed into the same address afterwards. Storing the
    S* would have given up both.

    THE static_cast IS AN INSTRUCTION, NOT A CHECK
    Three things have to hold, and they all trace back to attach_script:

      - The id pins the node. A resolving id is that same object, because
        unregister_node() zeroes the generation and register_node() hands out a
        fresh monotonic one. See NodeRef.hpp.
      - The node was holding an S when this handle was built. Both constructors
        below establish that, and both throw if they cannot.
      - The node still holds that S. This is Node::script_sealed, set on the
        first successful attach and never cleared.

    THAT LAST ONE IS THE FRAGILE PART. script() would be a dangling pointer the
    moment anything could clear a live node's script, and detach_script() was
    exactly that. If you ever give a node a way to drop or swap its script while
    it is alive, this class breaks - not loudly, just wrongly - and the fix is to
    add an identity check to script(), not to trust the seal again.

    WHAT IT DOES NOT DO
    It does not keep the script alive, and it says nothing about whether the
    node is still in the scene tree. A live handle to an orphan is still live.
    It also cannot outlive a script's own on_detached: that hook now fires from
    ~Node, so by the time it runs the node is already leaving.
*/
template<typename N, typename S>
class ScriptRef {
    static_assert(std::is_base_of<Node, N>::value, "N must be a Node type.");
    static_assert(detail::has_node_type<S>::value, "S must derive from Script<N>, not ScriptBase: see Core/Script.hpp.");

    // Permits ScriptRef<Node, S-of-Node2D> while rejecting ScriptRef<Node2D,
    // S-of-a-plain-Node>: the script claims the node type it was declared for,
    // and this is the check that the two halves agree.
    static_assert(std::is_base_of<N, typename S::node_type>::value,
        "N must be the node type S was declared for, or a base of it.");

public:
    ScriptRef() = default;

    // Discovers whatever S the owner holds right now. Throws if it holds none.
    // The null check has to come first: get_script_as would dereference owner.
    explicit ScriptRef(N* owner) : ScriptRef(owner, owner != nullptr ? owner->template get_script_as<S>() : nullptr) {}

    // Explicit identity. Throws if owner does not actually hold this script,
    // which is the mismatch that would otherwise turn script() into a bad cast.
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

        // Compared against the sealed slot directly rather than through
        // get_script_as<S>(), which builds a ScriptRef and would come back here.
        S* held = dynamic_cast<S*>(owner->script_raw());
        if (held != script) {
            throw std::invalid_argument(std::string("ScriptRef<") + typeid(N).name()
                + ", " + typeid(S).name() + ">: that script is not attached to that node.");
        }

        id_ = owner->instance_id();
    }

    // Null once the owner is destroyed. Cast-free: the id is the whole check,
    // and the seal is what makes the cast below sound.
    S* script() const {
        Node* node = detail::NodeRegistry::instance().resolve(id_);
        return node != nullptr ? static_cast<S*>(node->script_raw()) : nullptr;
    }

    // The owning node, or nullptr once it is gone. Cast-free for the same
    // reason NodeRef<N>::get() is.
    N* node() const {
        Node* node = detail::NodeRegistry::instance().resolve(id_);
        return node != nullptr ? static_cast<N*>(node) : nullptr;
    }

    // Present so that an empty ScriptRef has a way to exist at all. Not meant to
    // be built by hand: id_ defaults to INVALID_ID, which never resolves.
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
    Node::attach_script<S>() is declared in Nodes/Node.hpp and defined here,
    because returning a ScriptRef by value needs the complete type and this
    header includes Node.hpp. Same arrangement Node::for_each_child uses.
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
    Node::get_script_as<S>() is declared in Nodes/Node.hpp and defined here,
    because returning a ScriptRef by value needs the complete type and this header
    includes Node.hpp. Same arrangement the NodeRef-returning members use; see
    the end of Core/NodeRef.hpp.
*/
template<typename S>
ScriptRef<typename S::node_type, S> Node::get_script_as() const {
    static_assert(std::is_base_of<ScriptBase, S>::value, "S must be a ScriptBase type.");
    static_assert(detail::has_node_type<S>::value,
        "S must derive from Script<N>, not ScriptBase: see Core/Script.hpp.");

    S* typed = dynamic_cast<S*>(script.get());
    // A query, so "no such script" is an answer, not a failure. The empty ref
    // must carry INVALID_ID, not this node's id: an id that resolves would make
    // script() narrow a non-S to S* and report the mismatch as a match.
    if (typed == nullptr) {
        return ScriptRef<typename S::node_type, S>(detail::NodeRegistry::INVALID_ID, 0);
    }
    // The script's owner has to be the S::node_type ScriptRef expects. It is:
    // attach_script<S>() only ever binds to a node that dynamic_casts to
    // S::node_type, and the slot is sealed after one attach.
    return ScriptRef<typename S::node_type, S>(
        static_cast<typename S::node_type*>(const_cast<Node*>(this)), typed);
}

} // namespace guayaba

#endif // GUAYABA_CORE_SCRIPTREF_HPP
