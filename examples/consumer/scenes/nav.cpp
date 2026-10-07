#include "nav.hpp"

namespace demo {

Vec2 screen_size(Vec2 fallback) {
    if (!guayaba::Input::g_terminal) return fallback;
    return guayaba::Input::g_terminal->get_screen_size(fallback);
}

std::unique_ptr<guayaba::NodeButton> tab_button(const std::string& label, Vec2 position,
                                                bool selected) {
    // La pestaña activa va con fondo brillante y la inactiva, apagada.
    //
    // El nombre va en el constructor y no se renombra despues: este motor no
    // tiene set_name. add_child() resuelve las colisiones solo, así que dos
    // pestañas que se llamen igualconviven con "label" y "label@2".
    std::unique_ptr<guayaba::NodeButton> button =
        std::make_unique<guayaba::NodeButton>("tab_" + label, position,
                                             selected ? "BLACK" : "WHITE",
                                             selected ? "BRIGHT_CYAN" : "BRIGHT_BLACK",
                                             std::vector<std::string>{" " + label + " "});
    return button;
}

std::unique_ptr<guayaba::NodeButton> action_button(const std::string& name, Vec2 position,
                                                   const std::string& label,
                                                   std::function<void()> on_click) {
    auto button = std::make_unique<guayaba::NodeButton>(name, position, "BLACK",
                                                        "BRIGHT_WHITE",
                                                        std::vector<std::string>{" " + label + " "});
    // El boton ya no lleva set_on_click: tiene una señal "clicked" y se engancha
    // a ella. connect<> con el pack vacio porque la carga es void(), y sin owner
    // porque no lo hay que tener - la conexión vive en la tabla del propio boton,
    // así que desaparece con él, que es justo cuando dejar de oír tiene sentido.
    if (on_click) button->connect_signal<>("clicked", std::move(on_click));
    return button;
}

std::unique_ptr<guayaba::NodeUI> text_line(const std::string& name, Vec2 position,
                                           const std::string& content,
                                           const std::string& color) {
    return std::make_unique<guayaba::NodeUI>(name, position, color, "BLACK",
                                             std::vector<std::string>{content});
}

std::unique_ptr<guayaba::NodeUI> text_block(const std::string& name, Vec2 position,
                                            std::vector<std::string> content,
                                            const std::string& color) {
    return std::make_unique<guayaba::NodeUI>(name, position, color, "BLACK", std::move(content));
}

std::unique_ptr<guayaba::NodeSQ> panel(const std::string& name, Vec2 position, Vec2 size,
                                        const std::string& fill, const std::string& border) {
    std::unique_ptr<guayaba::NodeSQ> frame =
        std::make_unique<guayaba::NodeSQ>(name, position, size, border, fill);
    return frame;
}

std::unique_ptr<guayaba::NodeBox> block(const std::string& name, Vec2 position, Vec2 size,
                                        const std::string& fill) {
    return std::make_unique<guayaba::NodeBox>(name, position, size, fill);
}

} // namespace demo
