#ifndef GUAYABA_UI_UIPREFABS_HPP
#define GUAYABA_UI_UIPREFABS_HPP

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <guayaba/Core/Color.hpp>
#include <guayaba/Core/Input.hpp>
#include <guayaba/Core/Script.hpp>
#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/Node2D.hpp>
#include <guayaba/Nodes/NodeButton.hpp>
#include <guayaba/Nodes/NodeSQ.hpp>

namespace guayaba::pui {

namespace detail {

/*
    Drags a node around while the left button stays held. Lives on the title bar,
    which is what the click lands on.
*/
class DragToMove : public Script {
public:
    std::weak_ptr<Node2D> target;
    Vec2 grab_offset{0, 0};
    bool moving = false;

    void begin_drag() {
        auto node = target.lock();
        if (!node) return;
        moving = true;
        grab_offset = node->get_global_position() - Input::mouse_pos;
    }

    void on_process(double) override {
        if (!moving) return;
        // The release frame still applies the final position, then stops.
        if (!Input::l_click) moving = false;
        auto node = target.lock();
        if (node) node->set_global_position(Input::mouse_pos + grab_offset);
    }
};

/*
    Slides a handle along a one-cell-thick line and pushes the container by the
    resulting progress. h_slider and v_slider only differ on the axis, so they
    share this script rather than two near-identical lambdas.
*/
class AxisSlider : public Script {
public:
    enum class Axis { Horizontal, Vertical };

    Axis axis = Axis::Horizontal;
    int span = 0;    // full extent of the line, used for the hit test
    int limit = 0;   // furthest the handle may travel
    std::weak_ptr<NodeSQ> line;
    std::weak_ptr<Node2D> container;
    std::shared_ptr<int> offset;

    bool moving = false;

private:
    int along(Vec2 v) const { return axis == Axis::Horizontal ? v.x : v.y; }
    int across(Vec2 v) const { return axis == Axis::Horizontal ? v.y : v.x; }
    Vec2 with_along(Vec2 base, int value) const {
        return axis == Axis::Horizontal ? Vec2{value, base.y} : Vec2{base.x, value};
    }

public:
    void on_process(double) override {
        if (limit < 1) return;

        auto line_node = line.lock();
        auto container_node = container.lock();
        if (!line_node || !container_node) return;

        Vec2 line_origin = line_node->get_global_position();
        Vec2 mouse = Input::mouse_pos;

        // Along the axis the mouse must be within the span; across it, exactly on
        // the single cell the line occupies.
        if (across(mouse) == across(line_origin)
            && along(mouse) >= along(line_origin)
            && along(mouse) < (along(line_origin) + span)
            && Input::l_click_just_pressed) {
            moving = true;
        }

        if (!Input::l_click) moving = false;

        if (moving) {
            int handle = std::max(std::min(along(mouse), along(line_origin) + limit), along(line_origin));
            if (auto slider = get_owner_as<Node2D>()) {
                slider->set_global_position(with_along(line_origin, handle));
            }
        }

        auto slider_node = get_owner_as<Node2D>();
        if (!slider_node) return;
        float progress = static_cast<float>(along(slider_node->get_global_position())
                                            - along(line_node->get_local_position()))
                       / static_cast<float>(limit);
        Vec2 container_position = container_node->get_global_position();
        container_node->set_global_position(with_along(
            container_position, static_cast<int>(-progress * *offset)));
    }
};

} // namespace detail

inline std::shared_ptr<NodeSQ> confirm_dialog(const std::string& title, std::function<void()> on_confirm){
    std::shared_ptr<NodeSQ> square = std::make_shared<NodeSQ>("Cuadro", Vec2{30, 15}, Vec2{27, 6}, Color::RED, Color::RED);
    std::shared_ptr<NodeButton> label = std::make_shared<NodeButton>("label", Vec2{1, 1}, Color::BLACK, Color::RED, std::vector<std::string>{"         " + title + "         "});

    // The drag state lives in the script, so there is no shared_ptr<bool> to
    // share between the click handler and the per-frame callback.
    auto drag = std::make_shared<detail::DragToMove>();
    drag->target = square;

    label->set_on_click([drag](){ drag->begin_drag(); });
    label->attach_script(drag);

    const Vec2 home{30, 15};

    std::shared_ptr<NodeButton> cancel = std::make_shared<NodeButton>("cancel", Vec2{1, 2}, Color::BLACK, Color::RED, std::vector<std::string>{
        ".----------.",
        "| Cancelar |",
        "'----------'"});
    cancel->set_on_click([square, home](){square->set_local_position(home);});

    std::shared_ptr<NodeButton> accept = std::make_shared<NodeButton>("accept", Vec2{13, 2}, Color::BLACK, Color::RED, std::vector<std::string>{
        ".-----------.",
        "| Continuar |",
        "'-----------'"});
    accept->set_on_click([on_confirm, square, home](){square->set_local_position(home); on_confirm();});

    square->add_child(label);
    square->add_child(cancel);
    square->add_child(accept);

    return square;
}

inline std::shared_ptr<NodeSQ> h_slider(std::shared_ptr<Node2D> container, std::shared_ptr<int> offset, int size, const std::string& color_a, const std::string& color_b, const std::string& color_c, const std::string& color_d){
    std::shared_ptr<NodeSQ> line = std::make_shared<NodeSQ>("Line", Vec2{0, 0}, Vec2{size, 1}, color_c, color_d);
    std::shared_ptr<NodeButton> slider = std::make_shared<NodeButton>("slider_button", Vec2{0, 0}, color_a, color_b, std::vector<std::string>{
        "|/////|",});

    auto drag = std::make_shared<detail::AxisSlider>();
    drag->axis = detail::AxisSlider::Axis::Horizontal;
    drag->span = size;
    drag->limit = size - 7;
    drag->line = line;
    drag->container = container;
    drag->offset = offset;
    slider->attach_script(drag);

    line->add_child(slider);
    return line;
}

inline std::shared_ptr<NodeSQ> v_slider(std::shared_ptr<Node2D> container, std::shared_ptr<int> offset, int size, const std::string& color_a, const std::string& color_b, const std::string& color_c, const std::string& color_d) {
    std::shared_ptr<NodeSQ> line = std::make_shared<NodeSQ>("VLine", Vec2{0, 0}, Vec2{1, size}, color_c, color_d);

    std::shared_ptr<NodeButton> slider = std::make_shared<NodeButton>("v_slider_button", Vec2{0, 0}, color_a, color_b, std::vector<std::string>{
        "-","|","|","-"
    });

    // The v_slider is one cell wide, so its limit is the line length minus the
    // four rows the handle occupies.
    auto drag = std::make_shared<detail::AxisSlider>();
    drag->axis = detail::AxisSlider::Axis::Vertical;
    drag->span = size;
    drag->limit = size - 4;
    drag->line = line;
    drag->container = container;
    drag->offset = offset;
    slider->attach_script(drag);

    line->add_child(slider);
    return line;
}

} // namespace guayaba::pui

#endif // GUAYABA_UI_UIPREFABS_HPP