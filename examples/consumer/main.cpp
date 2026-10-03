/*
    Demo interactiva de GuayabaTUIEngine.

        #include <guayaba/Guayaba.hpp>
        using namespace guayaba;

    Patron recomendado por el motor:
    - Crear funciones que se encargan de construir las escenas.
    - Las funciones de la forma: std::shared_ptr<Node> crearEscena(){... return root;};
      En el main():
    - Crear la root como root = crearEscena();
    - Establecer la root como la escena activa con SceneManager::get_instance().change_scene(root);
    - Iniciar el bucle de ejecucion con SceneManager::get_instance().start_running();
      Al iniciar el bucle la ejecucion del main se mantendra en ese punto,
      cualquier logica adicional debe ser manejada por el contenido de la escena.
*/

#include <guayaba/Guayaba.hpp>

using namespace guayaba;

std::shared_ptr<Node> crearEscena() {
    auto root = std::make_shared<Node>("root");

    auto fondo = std::make_shared<NodeBox>("fondo", Vec2{0, 0}, Vec2{64, 20}, Color::BLUE);
    root->add_child(fondo);

    auto titulo = std::make_shared<NodeUI>("titulo", Vec2{4, 2},
        Color::BRIGHT_WHITE, Color::BLUE,
        std::vector<std::string>{"=== GuayabaTUIEngine ==="});
    fondo->add_child(titulo);

    auto contenido = std::make_shared<Node2D>("contenido", Vec2{4, 5});
    fondo->add_child(contenido);

    // Dialogo de confirmacion: se puede arrastrar desde la barra de titulo.
    auto dialogo = pui::confirm_dialog("Confirmar", [](){});
    contenido->add_child(dialogo);

    // Boton que termina el bucle principal.
    auto salir = std::make_shared<NodeButton>("salir", Vec2{40, 16},
        Color::BLACK, Color::BRIGHT_WHITE,
        std::vector<std::string>{
            ".--------.",
            "| Salir  |",
            "'--------'"});
    salir->set_on_click([](){
        SceneManager::get_instance().stop_running();
    });
    contenido->add_child(salir);

    return root;
}

int main() {
    std::shared_ptr<Node> root = crearEscena();

    SceneManager::get_instance().change_scene(root);
    SceneManager::get_instance().start_running();

    return 0;
}