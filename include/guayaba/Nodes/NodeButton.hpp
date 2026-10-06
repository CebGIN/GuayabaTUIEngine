#ifndef GUAYABA_NODES_NODEBUTTON_HPP
#define GUAYABA_NODES_NODEBUTTON_HPP

#include <string>
#include <vector>

#include <guayaba/Core/Input.hpp>
#include <guayaba/Core/NodeType.hpp>
#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/NodeUI.hpp>
#include <guayaba/Renderer/ConsoleRenderer.hpp>

namespace guayaba {

class NodeButton : public NodeUI{
protected:
    bool hovered = false;

public:
    NodeButton(const std::string& node_name, Vec2 node_position, std::string text_color_name, std::string background_color_name, std::vector<std::string> node_text);

    void update(double delta_time) override;

    NodeType get_type() const override;

    bool is_hovered();

    void update_hover(const Vec2 mouse_global_pos);

    // Emits the "clicked" signal declared in the constructor.
    void handle_click();

    void draw(ConsoleRenderer& renderer) override;
};

} // namespace guayaba

#endif // GUAYABA_NODES_NODEBUTTON_HPP