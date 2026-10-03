#ifndef GUAYABA_NODES_NODE2D_HPP
#define GUAYABA_NODES_NODE2D_HPP

#include <string>

#include <guayaba/Core/NodeType.hpp>
#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/Node.hpp>

namespace guayaba {

class Node2D : public Node{
protected:
    Vec2 position;
    mutable Vec2 cached_global_position;
    mutable bool is_cached_position_valid = false;

public:

    Node2D(const std::string& node_name = "Unnamed Node", Vec2 node_position = {0, 0});

    void invalidate_cache_recursively() override;

    Vec2 get_local_position() const;

    void set_local_position(Vec2 new_position);

    NodeType get_type() const override;

    Vec2 get_global_position() const;

    void set_global_position(Vec2 new_global_position);
};

} // namespace guayaba

#endif // GUAYABA_NODES_NODE2D_HPP