/*
    Demo interactiva de GuayabaTUIEngine.

        #include <guayaba/Guayaba.hpp>
        using namespace guayaba;

    Patron recomendado por el motor:
    - Crear funciones que se encargen de construir las escenas.
    - Las funciones devuelven std::unique_ptr<Node>: la funcion es la duena del
      nodo hasta que lo entrega, y el arbol es dueno a partir de ahi.
    - En el main():
    - Crear la root como root = crearEscena();
    - Establecer la root como la escena activa con
      SceneManager::get_instance().change_scene(std::move(root));
    - Iniciar el bucle de ejecucion con SceneManager::get_instance().start_running();
      Al iniciar el bucle la ejecucion del main se mantendra en ese punto,
      cualquier logica adicional debe ser manejada por el contenido de la escena.
*/

#include <guayaba/Guayaba.hpp>

using namespace guayaba;

std::unique_ptr<Node> crearEscena() {
    auto root = std::make_unique<Node>("root");

    root->add_child<NodeBox>("fondo", Vec2{0, 0}, Vec2{64, 20}, Color::BLUE);

    // add_child<> y get_child<> devuelven NodeRef<T>: guardan un id, no una
    // direccion, asi que un handle liberado responde vacio en vez de quedar
    // colgando. Para usarlos dentro del subarbol que los contiene basta get().
    const NodeRef<Node2D> fondo = root->get_child<Node2D>(0);

    fondo->add_child<NodeUI>("titulo", Vec2{4, 2},
        Color::BRIGHT_WHITE, Color::BLUE,
        std::vector<std::string>{"=== GuayabaTUIEngine ==="});

    const NodeRef<Node2D> contenido = fondo->add_child<Node2D>("contenido", Vec2{4, 5});

    // Boton que termina el bucle principal.
    const NodeRef<NodeButton> salir = contenido->add_child<NodeButton>("salir", Vec2{40, 16},
        Color::BLACK, Color::BRIGHT_WHITE,
        std::vector<std::string>{
            ".--------.",
            "| Salir  |",
            "'--------'"});
    salir->set_on_click([](){
        SceneManager::get_instance().stop_running();
    });

    return root;
}

int main() {
    std::unique_ptr<Node> root = crearEscena();

    SceneManager::get_instance().change_scene(std::move(root));
    SceneManager::get_instance().start_running();

    return 0;
}