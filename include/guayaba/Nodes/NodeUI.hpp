#ifndef GUAYABA_NODES_NODEUI_HPP
#define GUAYABA_NODES_NODEUI_HPP

#include <cstdint>
#include <string>
#include <vector>

#include <guayaba/Core/NodeType.hpp>
#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/Node2D.hpp>
#include <guayaba/Renderer/ConsoleRenderer.hpp>

namespace guayaba {

class NodeUI : public Node2D{
protected:
    Vec2 size;
    std::vector<std::string> text;
    uint16_t text_color;
    uint16_t background_color;

    void recalculate_size();

public:

    NodeUI(const std::string& node_name, Vec2 node_position, std::vector<std::string> node_text);

    NodeUI(const std::string& node_name, Vec2 node_position,
           const std::string& text_color_name, const std::string& background_color_name,
           std::vector<std::string> node_text);

    void set_text(std::vector<std::string> new_text);

    std::vector<std::string> get_text();

    NodeType get_type() const override;

    bool is_inside(const Vec2 point);

    void draw(ConsoleRenderer& renderer) override;

    void change_text_color(const std::string& text_color_name);

    void change_background_color(const std::string& background_color_name);

    Vec2 get_size() const;
};

} // namespace guayaba

#endif // GUAYABA_NODES_NODEUI_HPP