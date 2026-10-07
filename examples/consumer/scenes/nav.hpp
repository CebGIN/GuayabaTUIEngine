#ifndef GUAYABA_EXAMPLES_SCENES_NAV_HPP
#define GUAYABA_EXAMPLES_SCENES_NAV_HPP

#include <functional>
#include <memory>
#include <string>

#include <guayaba/Guayaba.hpp>
#include <guayaba/Core/NodeRegistry.hpp>

/*
    Contrato compartido por las escenas interactivas de la demo.

    Cada escena construye SOLO su contenido. La barra de pestanas y el boton de
    salir los pone main.cpp; la escena recibe un Nav si quiere
    botones para saltar a otra escena o para salir.
*/
namespace demo {

// Las escenas viven en este namespace y no quieren repetir guayaba:: en cada
// linea. El using se queda dentro de demo, asi que no sale de aqui.
using namespace guayaba;

struct Nav {
    std::function<void(int index)> goto_scene;
    std::function<void()> quit;
};

// Tamaño de pantalla util, con un minimo razonable para no dibujar fuera.
Vec2 screen_size(Vec2 fallback = {100, 60});

/*
    Handle de nodo a partir de un puntero recien creado.

    El NodeRef de este motor se construye directamente desde el puntero, asi que
    esto es solo una abreviatura. Lo que si cambia respecto al motor anterior es
    que el handle NO sabe que guion lleva: NodeRef<N> y ScriptRef<N, S> son dos
    cosas, y por eso quien necesite los dos guarda los dos.
*/
template<typename T>
guayaba::NodeRef<T> make_node_ref(guayaba::Node* node) {
    return guayaba::NodeRef<T>(static_cast<T*>(node));
}

// El guion de un nodo, como ScriptRef: el mismo objeto que devuelve
// attach_script, pero construido desde un handle que ya tienes.
//
// Devuelve nullptr si el nodo ya no esta o si no lleva ese guion, que es lo que
// hace que valgan las dos mitades de la comprobacion de abajo.
template<typename S, typename N>
guayaba::ScriptRef<typename S::node_type, S> script_of(const guayaba::NodeRef<N>& node) {
    N* alive = node.get();
    // ScriptRef no acepta un nodo nulo -su constructor desreferencia para leer el
    // id-, asi que el caso "el nodo ya no esta" se resuelve antes. En ese caso se
    // devuelve el handle vacio, que es lo que get_script_as devuelve cuando el
    // nodo vive pero no lleva ese guion: los dos casos dan nullptr desde script().
    return alive ? alive->template get_script_as<S>()
                 : guayaba::ScriptRef<typename S::node_type, S>();
}



// Azucar para lo mismo: "¿vive el nodo Y lleva este guion?". Encadena los dos
// porque los dos hacen falta, y leerlos separados en cada sitio invita a
// olvidar el segundo.
//
// Se apoya en ScriptRef, que es publico, en vez de en el script_raw() del nodo,
// que no lo es: el motor no quiere que una escena hurgue en el guion por la
// puerta de atrás.
template<typename S, typename N>
S* script_ptr(const guayaba::NodeRef<N>& node) {
    return script_of<S, N>(node).script();
}

// Un boton de la barra superior. Los autores de escenas no lo usan: lo pone main.
std::unique_ptr<guayaba::NodeButton> tab_button(const std::string& label, Vec2 position,
                                                bool selected);

// Boton de utilidad para el contenido de una escena: tiene sitio para el texto,
// acepta los nombres de color de guayaba::Color y no hace nada raro si no hay
// terminal.
std::unique_ptr<guayaba::NodeButton> action_button(const std::string& name, Vec2 position,
                                                   const std::string& label,
                                                   std::function<void()> on_click);

// Una linea de texto de una sola linea, con el color que se le pase.
std::unique_ptr<guayaba::NodeUI> text_line(const std::string& name, Vec2 position,
                                           const std::string& content,
                                           const std::string& color = "WHITE");

// Etiqueta multilinea: cada elemento del vector es una fila.
std::unique_ptr<guayaba::NodeUI> text_block(const std::string& name, Vec2 position,
                                            std::vector<std::string> content,
                                            const std::string& color = "WHITE");

// Marco vacio para agrupar visualmente.
std::unique_ptr<guayaba::NodeSQ> panel(const std::string& name, Vec2 position, Vec2 size,
                                        const std::string& fill = "BLACK",
                                        const std::string& border = "CYAN");

// Relleno plano de color.
std::unique_ptr<guayaba::NodeBox> block(const std::string& name, Vec2 position, Vec2 size,
                                        const std::string& fill);

} // namespace demo

#endif // GUAYABA_EXAMPLES_SCENES_NAV_HPP
