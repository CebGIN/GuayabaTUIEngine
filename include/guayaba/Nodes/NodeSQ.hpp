#ifndef GUAYABA_NODES_NODESQ_HPP
#define GUAYABA_NODES_NODESQ_HPP

#include <cstdint>
#include <string>

#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/Node2D.hpp>
#include <guayaba/Renderer/ConsoleRenderer.hpp>

namespace guayaba {

class NodeSQ : public Node2D{
protected:
    Vec2 size;
    uint16_t text_color;
    uint16_t background_color;
public:
    NodeSQ(const std::string& node_name, Vec2 node_position, Vec2 node_size,
        const std::string& text_color_name, const std::string& background_color_name);

    Vec2 get_size() const;

    void set_size(Vec2 new_size = {1, 1});

    void change_text_color(const std::string& text_color_name);

    void change_background_color(const std::string& background_color_name);

    void draw(ConsoleRenderer& renderer) override;
};

} // namespace guayaba

#endif // GUAYABA_NODES_NODESQ_HPP