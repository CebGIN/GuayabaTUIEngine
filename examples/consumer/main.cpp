/*
    Demo interactiva de GuayabaTUIEngine.

        cmake -S examples/consumer -B build/consumer -DGUAYABA_CONSUMER_MODE=add_subdirectory
        cmake --build build/consumer
        ./build/consumer

    Que se ve:

      Script Lab   Propiedades de un script leidas y escritas a traves del nodo que
                  lo tiene, con la clase del script nombrada una sola vez. Ciclo de
                  vida de los hooks, nodos que nacen y mueren, lookup de rutas.
      Tree Lab     Cambios de estructura en caliente: reparent en diferido, queue_free
                  en cascada, sliders y un dialogo arrastrable. Un log en pantalla
                  cuenta los hooks que dispara cada operacion.
      Cycles Lab   El caso que este motor cerro: callbacks que referencian al nodo
                  que las contiene. Al pulsar, el subarbol se libera entero y un
                  contador de nodos vivos lo demuestra en pantalla.

    Solo hay raton: el backend no lee teclado, asi que todo se maneja con clics.

    Como esta montado:

      - La construccion usa punteros crudos porque el codigo es dueno de lo que crea.
        make_node_ref los convierte en referencias, y a partir de ahi todo lo demas
        son NodeRef: referencias que no poseen nada y que avisan si quedan obsoletas.
      - La barra de pestanas la pone este fichero. Cada escena construye solo su
        contenido y recibe un Nav si quiere botones para saltar o para salir.
      - El contador de "nodos vivos" sale de NodeRegistry, que es donde el motor
        apunta cada nodo al construirse y lo borra al destruirlo.
*/

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <guayaba/Guayaba.hpp>

#include "scenes/cycles_lab.hpp"
#include "scenes/nav.hpp"
#include "scenes/script_lab.hpp"
#include "scenes/tree_lab.hpp"

using namespace guayaba;

namespace {

constexpr int TAB_Y = 1;
constexpr int CONTENT_Y = 4;

using SceneBuilder = std::unique_ptr<Node> (*)();

struct Tab {
    const char* label;
    SceneBuilder build;
};

const Tab TABS[] = {
    {"Script Lab", &demo::build_script_lab},
    {"Tree Lab", &demo::build_tree_lab},
    {"Cycles Lab", &demo::build_cycles_lab},
};

constexpr int TAB_COUNT = static_cast<int>(sizeof(TABS) / sizeof(TABS[0]));

int current_tab = 0;
demo::Nav nav;

/*
    Un boton de la barra, con su manejador puesto. El handler vive en el boton a
    traves de un handle, no en una lambda que lo capture: por eso esta barra no
    puede crear un ciclo.
*/
std::unique_ptr<NodeButton> tab_with_handler(const std::string& label, Vec2 position,
                                             bool selected, std::function<void()> handler) {
    std::unique_ptr<NodeButton> button = demo::tab_button(label, position, selected);
    // La pestaña lleva su handler en la señal "clicked" que declara su
    // constructor. connect<> con el pack vacío porque la carga es void().
    if (handler) button->connect_signal<>("clicked", std::move(handler));
    return button;
}

/*
    La escena activa: barra de pestanas arriba, contenido debajo. Se reconstruye
    entera en cada cambio de escena, que es justo lo que hay que ver funcionando.
*/
std::unique_ptr<Node> build_chrome(int selected) {
    auto root = std::make_unique<Node>("root");
    NodeRef<Node> root_ref = demo::make_node_ref<Node>(root.get());

    auto bar = std::make_unique<NodeBox>("barra", Vec2{0, 0}, Vec2{200, 3}, "BRIGHT_BLACK");
    NodeRef<Node> bar_ref = demo::make_node_ref<Node>(bar.get());
    root_ref->add_child(std::move(bar));

    int x = 2;
    for (int i = 0; i < TAB_COUNT; ++i) {
        const int index = i;
        const std::string label = TABS[i].label;
        const int width = static_cast<int>(label.size()) + 3;
        bar_ref->add_child(tab_with_handler(label, Vec2{x, TAB_Y}, index == selected,
                                            [index]{ nav.goto_scene(index); }));
        x += width;
    }

    bar_ref->add_child(tab_with_handler("Salir", Vec2{x, TAB_Y}, false, nav.quit));

    auto content = std::make_unique<Node2D>("contenido", Vec2{0, CONTENT_Y});
    NodeRef<Node2D> content_ref = demo::make_node_ref<Node2D>(content.get());
    root_ref->add_child(std::move(content));
    content_ref->add_child(TABS[selected].build());

    return root;
}

void change_tab(int index) {
    if (index < 0 || index >= TAB_COUNT || index == current_tab) return;
    current_tab = index;
    SceneManager::get_instance().change_scene(build_chrome(index));
}

} // namespace

int main() {
    nav.goto_scene = [](int index) { change_tab(index); };
    nav.quit = []() { SceneManager::get_instance().stop_running(); };

    std::cout << "GuayabaTUIEngine - demo\nTabs:";
    for (int i = 0; i < TAB_COUNT; ++i) std::cout << " " << TABS[i].label;
    std::cout << " Salir\nRaton para todo. Ctrl+C para cerrar.\n\n";

    SceneManager::get_instance().change_scene(build_chrome(current_tab));
    SceneManager::get_instance().start_running();

    return 0;
}
