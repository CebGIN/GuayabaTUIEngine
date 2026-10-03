#ifndef GUAYABA_NODES_NODEBUTTON_HPP
#define GUAYABA_NODES_NODEBUTTON_HPP

#include <functional>
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
    std::function<void()> on_click;

public:
    NodeButton(const std::string& node_name, Vec2 node_position, std::string text_color_name, std::string background_color_name, std::vector<std::string> node_text);

    void update(double delta_time) override;

    NodeType get_type() const override;

    bool is_hovered();

    void update_hover(const Vec2 mouse_global_pos);

    void handle_click();

    void set_on_click(std::function<void()> callback);

    void draw(ConsoleRenderer& renderer) override;
};

} // namespace guayaba

#endif // GUAYABA_NODES_NODEBUTTON_HPP