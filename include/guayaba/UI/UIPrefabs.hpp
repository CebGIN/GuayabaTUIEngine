#ifndef GUAYABA_UI_UIPREFABS_HPP
#define GUAYABA_UI_UIPREFABS_HPP

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <guayaba/Core/Color.hpp>
#include <guayaba/Core/Input.hpp>
#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/Node2D.hpp>
#include <guayaba/Nodes/NodeButton.hpp>
#include <guayaba/Nodes/NodeSQ.hpp>

namespace guayaba::pui {

inline std::shared_ptr<NodeSQ> confirm_dialog(const std::string& title, std::function<void()> on_confirm){
    std::shared_ptr<NodeSQ> square = std::make_shared<NodeSQ>("Cuadro", Vec2{30, 15}, Vec2{27, 6}, Color::RED, Color::RED);
    std::shared_ptr<NodeButton> label = std::make_shared<NodeButton>("label", Vec2{1, 1}, Color::BLACK, Color::RED, std::vector<std::string>{"         " + title + "         "});

    // Owned by the dialog itself rather than leaked on the heap.
    auto move = std::make_shared<bool>(false);
    auto offset = std::make_shared<Vec2>();

    label->set_on_click([move, offset, square](){
        *move = true;
        *offset = (square->get_global_position() - Input::mouse_pos);
    });
    label->set_process_function([square, move, offset](double){
        if(*move){
            if (!Input::l_click) *move = false;
            square->set_global_position(Input::mouse_pos + *offset);
        }
    });

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

    auto moving = std::make_shared<bool>(false);

    slider->set_process_function([line, slider, container, offset, size, moving](double){
        int limit = size - 7;
        if (limit < 1) return;

        if (Input::mouse_pos.y == line->get_global_position().y &&
            Input::mouse_pos.x >= line->get_global_position().x &&
            Input::mouse_pos.x < (line->get_global_position().x + size) &&
            Input::l_click_just_pressed) *moving = true;

        if (!Input::l_click) *moving = false;

        if (*moving) {
            int x = std::max(std::min(Input::mouse_pos.x, line->get_global_position().x + limit), line->get_global_position().x);
            slider->set_global_position(Vec2{x, line->get_global_position().y});
        }

        float progress = static_cast<float>(slider->get_global_position().x - line->get_local_position().x) / static_cast<float>(limit);
        container->set_global_position(Vec2{static_cast<int>(-progress * *offset), container->get_global_position().y});
    });
    line->add_child(slider);
    return line;
}

inline std::shared_ptr<NodeSQ> v_slider(std::shared_ptr<Node2D> container, std::shared_ptr<int> offset, int size, const std::string& color_a, const std::string& color_b, const std::string& color_c, const std::string& color_d) {
    std::shared_ptr<NodeSQ> line = std::make_shared<NodeSQ>("VLine", Vec2{0, 0}, Vec2{1, size}, color_c, color_d);

    std::shared_ptr<NodeButton> slider = std::make_shared<NodeButton>("v_slider_button", Vec2{0, 0}, color_a, color_b, std::vector<std::string>{
        "-","|","|","-"
    });

    auto moving = std::make_shared<bool>(false);

    slider->set_process_function([line, slider, container, offset, size, moving](double) {
        // The slider is one line tall, so the limit is the line length minus that height.
        int limit = size - 4;
        if (limit < 1) return;

        if (Input::mouse_pos.x == line->get_global_position().x &&
            Input::mouse_pos.y >= line->get_global_position().y &&
            Input::mouse_pos.y < (line->get_global_position().y + size) &&
            Input::l_click_just_pressed) *moving = true;

        if (!Input::l_click) *moving = false;

        if (*moving) {
            int y = std::max(std::min(Input::mouse_pos.y, line->get_global_position().y + limit), line->get_global_position().y);
            slider->set_global_position(Vec2{line->get_global_position().x, y});
        }

        float progress = static_cast<float>(slider->get_global_position().y - line->get_local_position().y) / static_cast<float>(limit);
        container->set_global_position(Vec2{container->get_global_position().x, static_cast<int>(-progress * *offset)});
    });

    line->add_child(slider);
    return line;
}

} // namespace guayaba::pui

#endif // GUAYABA_UI_UIPREFABS_HPP