#ifndef GUAYABA_EXAMPLES_SCENES_TREE_LAB_HPP
#define GUAYABA_EXAMPLES_SCENES_TREE_LAB_HPP

#include <algorithm>
#include <cstddef>
#include <exception>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <guayaba/Guayaba.hpp>

#include "nav.hpp"

/*
    Tree Lab: lo que pasa cuando el ARBOL cambia con el raton por encima.

    Todo lo de aqui ocurre en el limite de frame, no en el clic. Esa es la idea
    de la escena: el clic solo ANOTA una peticion, y el final del frame la aplica. Entre las
    dos cosas hay un frame entero, y durante ese frame el arbol sigue como
    estaba. Las cuatro secciones lo demuestran:

      1 REPARENT      Un cuadro rojo vive dentro de "Panel", con dos marcos al
                      lado, "A" y "B". Los botones "-> A" y "-> B" (o el clic
                      derecho, que alterna) piden el movimiento. En la misma
                      pulsacion la linea de estado dice PENDIENTE y el padre
                      sigue siendo el viejo: todavia no ha pasado nada. En el
                      frame siguiente, tras el borrado, dice APLICADO y el padre
                      es el nuevo.
                      Las dos posiciones se muestran a la vez: la global NO se
                      mueve (el nodo sigue en el mismo sitio de la pantalla) y
                      la local SI, porque se recalcula contra el padre nuevo.

      2 HOOKS         perform_reparent llama a exit_tree y luego a enter_tree
                      aunque el nodo nunca saliera del arbol (paridad Godot).
                      Es la razon de ser del LOG DE HOOKS de la derecha: un
                      script pegado al cuadro rojo y otro a cada pieza del
                      subarbol escriben ahi sus entradas y sus salidas, y se ve
                      el orden exacto y cuando cae respecto al clic.

      3 queue_free    "liberar" marca un subarbol entero para destruir; el
                      trabajo lo hace el final del frame siguiente. "liberar x2" repite la
                      peticion para teachar que es idempotente. "recrear" monta
                      un subarbol nuevo, con handles nuevos, y el handle viejo
                      se queda obsoleto a proposito (valid() == false). Las
                      dos lineas de recuento salen de get_child_count() y de un
                      recuento recursivo sobre get_child(i), andando con handles.

      4 RUTAS         Seis lineas vivas de get_node_or_null: el nodo esta
                      ahora donde la ruta lo busca, o "-" si no; y el fallo
                      deliberado de siempre, "Panel/Fantasma", sale escrito como
                      "no encontrado" sin lanzar nada.

    LO QUE NO ESTA, Y POR QUE
    Abajo a la derecha no hay ni un h_slider ni un dialogo de pui. La otra linea de
    trabajo traia include/guayaba/UI/UIPrefabs.hpp con pui::h_slider() y
    pui::confirm_dialog(), y este motor no la tiene: es codigo del MOTOR, no de la
    demo. Reproducir un slider y un dialogo arrastrable aqui seria escribir una
    libreria de UI dentro de un ejemplo para que el ejemplo cuadre, que es al
    reves de como debe ir.

    El boton de reparent tampoco es el reparent_to() de una llamada que habia alla:
    aqui son remove_child() y add_child(), dos pasos, con el unique_ptr en medio
    para poder devolver el nodo a su padre si el destino lo rechaza. Un solo paso
    no podria recuperarse de ese fallo.

    En su lugar, "find_first_of_type<NodeSQ>()" es un recorrido propio de la escena
    por get_child(i), porque el motor no lo trae.

    Solo el raton: la escena solo lee mouse_pos y r_click_just_pressed, y los
    botones que pone nav usan l_click y l_click_just_pressed. Sin terminal no
    hay raton, asi que la escena se dibuja igual y no hace nada.

    Los textos son ASCII y se recortan al ancho de su columna, porque el motor
    no envuelve lineas: un recorte con "..." informa mas que desbordarse encima
    del panel de al lado.
*/
namespace demo {

namespace tree_lab_detail {

// -- texto ------------------------------------------------------------------

// Recorta a width, marcando el recorte. La columna manda: el motor no envuelve.
inline std::string fit(const std::string& text, int width) {
    if (width < 1) return std::string();
    if (static_cast<int>(text.size()) <= width) return text;
    if (width <= 3) return text.substr(0, static_cast<size_t>(width));
    return text.substr(0, static_cast<size_t>(width - 3)) + "...";
}

inline std::string num(int value) { return std::to_string(value); }

// "3,7" para una posicion, que cabe en cualquier columna.
inline std::string point(guayaba::Vec2 value) { return num(value.x) + "," + num(value.y); }

// El nombre si el handle acierta, "-" si no: una ruta que falla es una
// respuesta, no un error, y por eso se puede pintar en la misma linea.
inline std::string named(const guayaba::NodeRef<guayaba::Node>& handle) {
    return handle ? handle->get_name() : std::string("-");
}

// Recuento recursivo caminando con handles, no con punteros: cada get_child(i)
// devuelve un handle que caduca solo si el nodo ha muerto, que es justo lo
// que pasa si el borrado se cuela por medio.
inline int count_nodes(guayaba::NodeRef<guayaba::Node> node, int depth = 0) {
    if (!node.is_valid() || depth > 12) return 0;
    int total = 1;
    const int kids = static_cast<int>(node->get_child_count());
    for (int i = 0; i < kids; ++i) {
        total += count_nodes(node->get_child(static_cast<size_t>(i)), depth + 1);
    }
    return total;
}

// -- el log de hooks, que es a la vez script y nodo --------------------------

/*
    Vive en un NodeUI y se edita a si mismo. Otro nodo escribe en el log con un
    NodeRef al nodo que lo tiene, no con un puntero, y lo busca con
    get_script_as<S>(), que va vacio en vez de lanzar porque quien escribe es un
    hook.
*/
class EventLog : public guayaba::Script<guayaba::NodeUI> {
public:
    int visible = 12;   // cuantas lineas caben en la columna
    int width = 40;     // ancho de la columna, para el recorte
    std::vector<std::string> lines;

    void push(const std::string& line) {
        const size_t room = visible > 0 ? static_cast<size_t>(visible) : 1;
        lines.push_back(fit(line, width));
        while (lines.size() > room) lines.erase(lines.begin());
        redraw();
    }

    void redraw() {
        guayaba::NodeUI* out = get_owner_as<guayaba::NodeUI>();
        if (!out) return;
        out->set_text(lines.empty() ? std::vector<std::string>{"(vacio)"} : lines);
    }

    void on_enter_tree() override { push("log ENTER"); }
    void on_exit_tree() override { push("log EXIT"); }
};

// -- el script que ve las entradas y salidas del arbol -----------------------

/*
    Se pega a todo nodo cuya transicion interese (el cuadro rojo y cada pieza
    del subarbol). Escribe una linea por hook, y por eso el log enseña el orden
    global: al liberar, los hijos salen antes que el padre; al entrar, el padre
    entra antes que los hijos.
*/
// Script<Node>, no Script<NodeSQ>: este guion se engancha a un Node2D, a un
// NodeSQ y a un Node llano, y el chequeo de bind_to es un dynamic_cast al tipo
// declarado. Con NodeSQ el enganche al Node2D no pasaria.
class Lifewatch : public guayaba::Script<guayaba::Node> {
public:
    Lifewatch(std::string node_label, guayaba::NodeRef<guayaba::Node> log)
        : label(std::move(node_label)), sink(log) {}

    std::string label = "nodo";
    guayaba::NodeRef<guayaba::Node> sink;

    void on_enter_tree() override { note("ENTER"); }
    void on_exit_tree() override { note("EXIT"); }

    // El primer nodo del subarbol que se llama asi, o "-" si no hay ninguno.
    // Sustituye a un find_first_of_type<T>() que el motor no tiene: recorrer hijos
    // con get_child<T>(i) es cosa de la escena, no del motor.
    static std::string nombre_del_primer_nodo(const guayaba::NodeRef<guayaba::Node>& from,
                                              const std::string& wanted, int depth = 0) {
        if (!from.is_valid() || depth > 12) return "-";
        if (from->get_name() == wanted) return from->get_name();
        const int kids = static_cast<int>(from->get_child_count());
        for (int i = 0; i < kids; ++i) {
            // get_child<Node>, no get_child<Node2D>: este subarbol tiene hijos que
            // no son Node2D, y pedir Node2D haria que el static_cast de NodeRef
            // apuntara al tipo equivocado. Solo hace falta el nombre, asi que
            // basta con el tipo base - y el invariante de NodeRef se sostiene.
            const std::string found =
                nombre_del_primer_nodo(from->get_child<guayaba::Node>(static_cast<size_t>(i)),
                                       wanted, depth + 1);
            if (found != "-") return found;
        }
        return "-";
    }

private:
    void note(const std::string& what) {
        if (EventLog* log = script_ptr<EventLog>(sink)) log->push(label + " " + what);
    }
};

// -- el reparent en diferido --------------------------------------------------

/*
    El nodo que pide los movimientos y los lee. La peticion es de lo unico que
    se escribe en un clic; el resto es lectura, cada frame, por handles.

    Ojo con el orden: un clic marca la peticion dentro del update del boton, el
    el borrado se aplica DESPUES del draw, asi que el texto "PENDIENTE" se pinta en la
    misma imagen en la que se ha pulsado y "APLICADO" solo en la siguiente. Esa
    diferencia es la escena entera.
*/
class Rope : public guayaba::Script<guayaba::Node2D> {
public:
    Rope(guayaba::NodeRef<guayaba::Node> tree, guayaba::NodeRef<guayaba::NodeBox> node,
         guayaba::NodeRef<guayaba::Node2D> home, guayaba::NodeRef<guayaba::Node2D> into_a,
         guayaba::NodeRef<guayaba::Node2D> into_b, guayaba::NodeRef<guayaba::NodeUI> log)
        : scene(tree), rojo(node), panel(home), slotA(into_a), slotB(into_b), log_node(log) {}

    // Handles, no punteros: este script no posee nada.
    guayaba::NodeRef<guayaba::Node> scene;        // la raiz de la escena, para get_node
    guayaba::NodeRef<guayaba::NodeBox> rojo;      // el nodo que se mueve
    guayaba::NodeRef<guayaba::Node2D> panel;      // "Panel", el padre de origen
    guayaba::NodeRef<guayaba::Node2D> slotA;      // marco A
    guayaba::NodeRef<guayaba::Node2D> slotB;      // marco B
    guayaba::NodeRef<guayaba::Node> log_node;

    guayaba::NodeRef<guayaba::NodeUI> mark_a;
    guayaba::NodeRef<guayaba::NodeUI> mark_b;
    guayaba::NodeRef<guayaba::NodeUI> line_req;
    guayaba::NodeRef<guayaba::NodeUI> line_parent;
    guayaba::NodeRef<guayaba::NodeUI> line_pos;
    guayaba::NodeRef<guayaba::NodeUI> line_paths;

    bool wide = false;
    int path_width = 20;
    int mark_width = 6;
    int frames = 0;

    // Estado de la peticion en curso.
    bool waiting = false;
    std::string wanted;
    std::string was;
    std::string seen = "Panel";
    std::string request_line = "idle";   // la repinta paint()

    // -- las tres peticiones que caben en un boton --------------------------
    void request_a() { ask(slotA, "A"); }
    void request_b() { ask(slotB, "B"); }
    void request_home() { ask(panel, "Panel"); }

    // El clic derecho alterna entre los dos marcos, y de vuelta a casa.
    void toggle() {
        if (seen == "A") request_b();
        else if (seen == "B") request_home();
        else request_a();
    }

    void on_process(double) override {
        ++frames;

        // Sin terminal no hay raton, asi que ni se pregunta. Con terminal, el
        // boton derecho alterna el destino.
        if (guayaba::Input::g_terminal && guayaba::Input::r_click_just_pressed) {
            toggle();
        }

        paint();
        read_paths();
    }

    // El primer nodo del subarbol que se llama asi, o "-" si no hay ninguno.
    // Sustituye a un find_first_of_type<T>() que el motor no tiene: recorrer hijos
    // con get_child<T>(i) es cosa de la escena, no del motor.
    static std::string nombre_del_primer_nodo(const guayaba::NodeRef<guayaba::Node>& from,
                                              const std::string& wanted, int depth = 0) {
        if (!from.is_valid() || depth > 12) return "-";
        if (from->get_name() == wanted) return from->get_name();
        const int kids = static_cast<int>(from->get_child_count());
        for (int i = 0; i < kids; ++i) {
            // get_child<Node>, no get_child<Node2D>: este subarbol tiene hijos que
            // no son Node2D, y pedir Node2D haria que el static_cast de NodeRef
            // apuntara al tipo equivocado. Solo hace falta el nombre, asi que
            // basta con el tipo base - y el invariante de NodeRef se sostiene.
            const std::string found =
                nombre_del_primer_nodo(from->get_child<guayaba::Node>(static_cast<size_t>(i)),
                                       wanted, depth + 1);
            if (found != "-") return found;
        }
        return "-";
    }

private:
    void ask(const guayaba::NodeRef<guayaba::Node2D>& destination, const std::string& label) {
        guayaba::NodeBox* node = rojo.get();
        guayaba::Node* target = destination.get();
        if (!node || !target) return;

        guayaba::Node* current = node->get_parent().get();
        if (current == target) {
            request_line = wide ? (label + ": ya estaba ahi") : std::string("ya ahi");
            return;
        }

        // Este motor no tiene reparent_to(), y no se anade: el movimiento entre
        // padres es remove_child() y add_child(), dos pasos, a proposito. Un solo
        // metodo que reparenta no puede producir un huerfano que se olvide de
        // liberar; estos dos pueden, y por eso hay que comprobarlos.
        //
        // Y que el remove_child devuelva el unique_ptr es lo que permite decidir
        // antes de soltarlo: si el add falla, el nodo sigue en nuestras manos en
        // vez de estar en ninguno.
        std::unique_ptr<guayaba::Node> moving;
        try {
            moving = node->remove_child(node->child_index());
        } catch (const std::exception&) {
            request_line = wide ? (label + ": peticion rechazada") : std::string("rechazado");
            note("sacar de " + label + " rechazado");
            return;
        }
        if (!moving) {
            request_line = wide ? (label + ": peticion rechazada") : std::string("rechazado");
            note("sacar de " + label + " rechazado");
            return;
        }
        try {
            destination->add_child(std::move(moving));
        } catch (const std::exception&) {
            // El add ha fallado y `moving` ya no vale: se devuelve al padre viejo
            // para no perder el nodo. Es exactamente el fallo que un reparent_to
            // de un solo paso no podria tener.
            request_line = wide ? (label + ": destino rechazado") : std::string("rechazado");
            note("destino " + label + " rechazado");
            return;
        }

        // La peticion esta registrada. El padre SIGUE siendo el viejo: por eso
        // se guarda como estaba, para poder compararlo en el frame siguiente.
        was = current ? current->get_name() : "?";
        wanted = label;
        waiting = true;
        note("clic pide " + was + " -> " + label);

        // Y se repinta YA, sin esperar al siguiente on_process: el nodo que
        // lleva la logica se recorre antes que los botones, asi que si se
        // dejara para el frame siguiente el humano veria "APLICADO" en la misma
        // imagen en la que ha pulsado, y la escena no probaria nada. Pintando
        // desde aqui, la imagen del clic muestra PENDIENTE y el padre viejo.
        paint();
    }

    void paint() {
        guayaba::NodeBox* node = rojo.get();
        if (!node) return;

        guayaba::Node* parent = node->get_parent().get();
        const std::string now = parent ? parent->get_name() : "?";

        if (waiting && now != was) {
            // El final del frame ya paso: el padre cambio, asi que la peticion se dio.
            waiting = false;
            request_line = wide ? ("borrado: " + was + " -> " + now + " APLICADO")
                                : (now + " APLICADO");
        } else if (waiting) {
            // Sigue igual: estamos dentro del mismo frame que el clic.
            request_line = wide ? ("clic -> " + wanted + ": PENDIENTE (sigue en " + now + ")")
                                : std::string("PENDIENTE");
        }

        if (now != seen) {
            seen = now;
            // El global no se mueve al reparentarse, asi que el color es lo
            // unico que cambia en la pantalla: donde esta el padre de verdad.
            if (now == "A") node->change_background_color(guayaba::Color::BRIGHT_MAGENTA);
            else if (now == "B") node->change_background_color(guayaba::Color::BRIGHT_YELLOW);
            else node->change_background_color(guayaba::Color::BRIGHT_RED);
            note("padre ahora " + now);
        }

        const guayaba::Vec2 global_position = node->get_global_position();
        const guayaba::Vec2 local_position = node->get_local_position();

        if (guayaba::NodeUI* out = line_req.get()) {
            out->set_text({fit(request_line, text_width())});
        }
        if (guayaba::NodeUI* out = line_parent.get()) {
            out->set_text({fit(wide ? "padre: " + now + "   global: (" + point(global_position) + ")"
                                   : "padre:" + now,
                                 text_width())});
        }
        if (guayaba::NodeUI* out = line_pos.get()) {
            // El par global/local es el resultado pedido: el global intacto y
            // la local recalculada contra el padre nuevo. En columna estrecha
            // no caben los dos con su etiqueta larga, asi que van abreviados.
            out->set_text({fit(wide ? "local: (" + point(local_position) + ")   (el global no se mueve)"
                                   : "g" + point(global_position) + " l" + point(local_position),
                                 text_width())});
        }
        if (guayaba::NodeUI* out = mark_a.get()) {
            out->set_text({fit(now == "A" ? "*Rojo*" : "  -  ", mark_width)});
        }
        if (guayaba::NodeUI* out = mark_b.get()) {
            out->set_text({fit(now == "B" ? "*Rojo*" : "  -  ", mark_width)});
        }
    }

    void read_paths() {
        guayaba::Node* tree = scene.get();
        guayaba::NodeUI* out = line_paths.get();
        if (!tree || !out) return;

        // get_node no lanza: un acierto es un handle y un fallo es un handle
        // vacio. Las tres primeras lineas siguen al nodo de verdad; la ultima
        // se escribe partida en dos porque el fallo se tiene que LEER entero.
        const guayaba::NodeRef<guayaba::Node> at_home = tree->get_node_or_null<guayaba::Node>("Panel/Rojo");
        const guayaba::NodeRef<guayaba::Node> in_a = tree->get_node_or_null<guayaba::Node>("Panel/A/Rojo");
        const guayaba::NodeRef<guayaba::Node> in_b = tree->get_node_or_null<guayaba::Node>("Panel/B/Rojo");
        const guayaba::NodeRef<guayaba::NodeSQ> typed = tree->get_node_or_null<guayaba::NodeSQ>("Panel/A");
        const guayaba::NodeRef<guayaba::Node> ghost = tree->get_node_or_null<guayaba::Node>("Panel/Fantasma");

        std::vector<std::string> lines;
        lines.push_back(fit("Panel/Rojo -> " + named(at_home), path_width));
        lines.push_back(fit("Panel/A/Rojo -> " + named(in_a), path_width));
        lines.push_back(fit("Panel/B/Rojo -> " + named(in_b), path_width));
        lines.push_back(fit("as<SQ> Panel/A -> " + named(typed), path_width));
        lines.push_back(fit("Panel/Fantasma ->", path_width));
        lines.push_back(ghost ? "  (esto no deberia)" : "  no encontrado");
        out->set_text(lines);
    }

    // El ancho de las tres lineas de estado. En columna estrecha el dialogo del
    // prefab tapa desde la columna 12, asi que el texto se queda en 11 y se lee
    // entero en vez de cortado a media palabra.
    int text_width() const { return wide ? 58 : 11; }

    void note(const std::string& what) {
        if (EventLog* log = script_ptr<EventLog>(log_node)) log->push("rojo " + what);
    }
};

// -- queue_free en cascada, idempotente y recreable ---------------------------

/*
    Los tres botones de la seccion 3 escriben aqui, y los handles se guardan como
    datos: "stale" es el handle del subarbol anterior, que se conserva a
    proposito para poder teaching que valid() se ha puesto a false sin que
    nadie guardase un puntero colgando.
*/
class SubtreeOps : public guayaba::Script<guayaba::Node2D> {
public:
    SubtreeOps(guayaba::NodeRef<guayaba::Node2D> parent_rail, guayaba::NodeRef<guayaba::Node> log)
        : rail(parent_rail), log_node(log) {}

    guayaba::NodeRef<guayaba::Node2D> rail;      // donde vive la banda del subarbol
    guayaba::NodeRef<guayaba::Node> log_node;
    guayaba::NodeRef<guayaba::NodeUI> line_one;
    guayaba::NodeRef<guayaba::NodeUI> line_two;
    guayaba::NodeRef<guayaba::NodeUI> line_tag;  // el contador pintado en la banda

    guayaba::NodeRef<guayaba::Node2D> subtree;   // el subarbol vivo
    guayaba::NodeRef<guayaba::Node2D> stale;     // el handle del anterior

    int generation = 0;
    int text_width = 15;
    bool marked = false;   // ya hay una peticion de liberate sin resolver

    // -- los tres botones ----------------------------------------------------

    void release() {
        guayaba::Node2D* live = subtree.get();
        if (!live) {
            note("lib: ya no hay");
            marked = false;
            return;
        }
        if (marked) {
            // El nodo sigue en el arbol, pero ya estaba pedido: queue_free es
            // idempotente y aqui se ve sin depender de lo que haga el borrado.
            note("lib: ya marcado, no-op");
            return;
        }
        // El handle viejo se guarda antes de soltar el subarbol: es el que se
        // quedara obsoleto, y se enseña en la linea de recuento.
        stale = subtree;
        marked = true;
        note("lib: " + num(count_nodes(subtree)) + " nodos");
        live->queue_free();   // solo marca: el final del frame hace el trabajo
    }

    void release_twice() {
        release();
        release();             // idempotente: la segunda peticion no cambia nada
        note("lib x2: no-op");
    }

    void recreate() {
        guayaba::Node2D* parent = rail.get();
        if (!parent) return;
        marked = false;

        if (guayaba::Node2D* live = subtree.get()) {
            stale = subtree;
            live->queue_free();
        }

        ++generation;
        guayaba::NodeRef<guayaba::Node2D> fresh;
        // Se construye con unique_ptr, se toman los handles mientras se es
        // dueno, y el dueno se entrega al arbol. A partir de aqui, nada de
        // punteros crudos.
        parent->add_child(spawn(generation, fresh));
        subtree = fresh;
        note("nuevo: Sub" + num(generation));
    }

    void on_process(double) override {
        const bool live = subtree.is_valid();
        if (!live) marked = false;
        int kids = -1;
        int total = -1;
        std::string first_square = "-";

        if (guayaba::Node2D* sub = subtree.get()) {
            kids = static_cast<int>(sub->get_child_count());
            total = count_nodes(subtree);
            // El primer nodo con ese nombre, bajando el subarbol con handles.
            first_square = nombre_del_primer_nodo(subtree, "hijo1");
        }

        if (guayaba::NodeUI* out = line_one.get()) {
            const std::string line = live ? ("hijos " + num(kids) + " total " + num(total))
                                          : std::string("subarbol liberado");
            out->set_text({fit(line, text_width)});
        }
        if (guayaba::NodeUI* out = line_two.get()) {
            // stale.is_valid() en pantalla: el handle del subarbol viejo sigue
            // escribiendose en la escena y ya no resuelve a nada.
            const std::string line = std::string("viejo ") + (stale.is_valid() ? "1" : "0")
                                     + " sq " + first_square;
            out->set_text({fit(line, text_width)});
        }
        if (guayaba::NodeUI* out = line_tag.get()) {
            const std::string tag = "n=" + num(live ? total : 0);
            out->set_text({fit(tag, 4)});
        }
    }

    // El primer nodo del subarbol que se llama asi, o "-" si no hay ninguno.
    // Sustituye a un find_first_of_type<T>() que el motor no tiene: recorrer hijos
    // con get_child<T>(i) es cosa de la escena, no del motor.
    static std::string nombre_del_primer_nodo(const guayaba::NodeRef<guayaba::Node>& from,
                                              const std::string& wanted, int depth = 0) {
        if (!from.is_valid() || depth > 12) return "-";
        if (from->get_name() == wanted) return from->get_name();
        const int kids = static_cast<int>(from->get_child_count());
        for (int i = 0; i < kids; ++i) {
            // get_child<Node>, no get_child<Node2D>: este subarbol tiene hijos que
            // no son Node2D, y pedir Node2D haria que el static_cast de NodeRef
            // apuntara al tipo equivocado. Solo hace falta el nombre, asi que
            // basta con el tipo base - y el invariante de NodeRef se sostiene.
            const std::string found =
                nombre_del_primer_nodo(from->get_child<guayaba::Node>(static_cast<size_t>(i)),
                                       wanted, depth + 1);
            if (found != "-") return found;
        }
        return "-";
    }

private:
    // Un subarbol nuevo: cuatro nodos, uno de ellos plano y sin dibujar, para
    // que el recuento y find_first_of_type noCoincidan entre si.
    std::unique_ptr<guayaba::Node2D> spawn(int tag,
                                           guayaba::NodeRef<guayaba::Node2D>& handle_out) {
        const std::string name = "Sub" + num(tag);
        auto root = std::make_unique<guayaba::Node2D>(name, guayaba::Vec2{1, 0});
        guayaba::NodeRef<guayaba::Node2D> root_ref = make_node_ref<guayaba::Node2D>(root.get());
        root_ref->attach_script<Lifewatch>(name, log_node);

        add_cell(root_ref, "caja1", guayaba::Color::BRIGHT_GREEN, guayaba::Vec2{0, 0});
        add_cell(root_ref, "caja2", guayaba::Color::BRIGHT_BLUE, guayaba::Vec2{5, 0});
        add_blank(root_ref, "hoja");

        handle_out = root_ref;
        return root;
    }

    void add_cell(const guayaba::NodeRef<guayaba::Node2D>& parent, const std::string& name,
                  const std::string& fill, guayaba::Vec2 at) {
        auto cell = std::make_unique<guayaba::NodeSQ>(name, at, guayaba::Vec2{4, 1},
                                                      guayaba::Color::BLACK, fill);
        guayaba::NodeRef<guayaba::NodeSQ> cell_ref = make_node_ref<guayaba::NodeSQ>(cell.get());
        cell_ref->attach_script<Lifewatch>(name, log_node);
        parent->add_child(std::move(cell));
    }

    void add_blank(const guayaba::NodeRef<guayaba::Node2D>& parent, const std::string& name) {
        auto blank = std::make_unique<guayaba::Node>(name);
        guayaba::NodeRef<guayaba::Node> blank_ref = make_node_ref<guayaba::Node>(blank.get());
        blank_ref->attach_script<Lifewatch>(name, log_node);
        parent->add_child(std::move(blank));
    }

    void note(const std::string& what) {
        if (EventLog* log = script_ptr<EventLog>(log_node)) log->push("sub " + what);
    }
};

} // namespace tree_lab_detail

/*
    La escena entera. Se construye con unique_ptr + add_child (quien construye es
    dueno de lo que crea) y en cuanto existe el puntero crudo se convierte en
    handle con make_node_ref. A partir de ahi, en las lambdas y en los scripts,
    solo hay NodeRef.

    La construccion mide la pantalla con demo::screen_size() y reparte dos
    columnas: a la izquierda el reparent y el subarbol, a la derecha las rutas,
    el slider y el log de hooks. Con 40x20 (el minimo) las tres secciones caben
    justa, los botones de liberar se apilan en tres filas y el dialogo se coloca
    encima de las rutas; con una pantalla ancha las columnas se separan, los
    botones comparten fila, el log crece a sus doce lineas y el dialogo baja
    hasta no taparlo.

    main.cpp cuelga el contenido 4 filas mas abajo, asi que aqui las coordenadas
    son de escena y el desplazamiento lo suma el motor al pintar.
*/
inline std::unique_ptr<guayaba::Node> build_tree_lab() {
    const guayaba::Vec2 screen = demo::screen_size({100, 60});
    // main.cpp baja el contenido 4 filas: el area util son screen.y - 4.
    const int rows = std::max(screen.y - 4, 16);
    const int lx = 1;

    // Columna izquierda ancha, pero sin comerse la derecha: la columna de las
    // rutas necesita 25 columnas para poder escribir "no encontrado".
    int left_w = std::min(std::max(screen.x / 2 - 2, 15), 58);
    left_w = std::min(left_w, std::max(screen.x - 25, 15));
    const int right_x = left_w + 2;
    const int right_w = std::max(screen.x - right_x - 1, 12);

    // Por debajo de 22 columnas los tres botones de la seccion 3 no caben en una
    // sola fila y se apilan.
    const bool wide = left_w >= 22;
    const int path_width = std::min(right_w, 46);
    const int oy = rows > 17 ? std::min((rows - 17) / 2, 6) : 0;
    const int log_lines = std::min(std::max(rows - oy - 9, 4), 12);

    // Los dos marcos, uno al lado del otro, con una columna entre ellos.
    const int panel_w = std::max((left_w - 1) / 2, 4);
    const int panel_b_w = std::max(left_w - 1 - panel_w, 4);
    const int mark_width = std::max(panel_w - 1, 4);

    // (Aqui vivia la colocacion del dialogo de pui, que se ha ido con el
    // prefab: ver el nota de "No hay slider ni dialogo" mas abajo.)

    auto scene = std::make_unique<guayaba::Node2D>("tree_lab", guayaba::Vec2{0, 0});
    guayaba::NodeRef<guayaba::Node2D> scene_ref = make_node_ref<guayaba::Node2D>(scene.get());

    // -- el log, primero: los demas scripts escriben en el --------------------
    auto log_node = demo::text_block("log", guayaba::Vec2{right_x, oy + 9}, {"(vacio)"},
                                     guayaba::Color::BRIGHT_CYAN);
    guayaba::NodeRef<guayaba::NodeUI> log_ref = make_node_ref<guayaba::NodeUI>(log_node.get());
    scene_ref->add_child(std::move(log_node));

    auto log_handle = log_ref->attach_script<tree_lab_detail::EventLog>();
    log_handle->visible = log_lines;
    log_handle->width = path_width;

    // -- seccion 1: el nodo que se muda ---------------------------------------
    // "Panel" no dibuja nada: agrupa los dos marcos y al cuadro rojo, de modo
    // que "Panel/Rojo", "Panel/A/Rojo" y "Panel/B/Rojo" son rutas de verdad, y
    // el cuadro tiene un padre de origen al que volver.
    auto panel_node = std::make_unique<guayaba::Node2D>("Panel", guayaba::Vec2{lx, oy + 6});
    guayaba::NodeRef<guayaba::Node2D> panel_ref = make_node_ref<guayaba::Node2D>(panel_node.get());
    scene_ref->add_child(std::move(panel_node));

    // Los dos destinos, uno al lado del otro, con una columna entre ellos. No
    // van en {0,0}: si compartieran posicion con Panel, la mudanza a A no
    // recalcularia nada y el ejemplo de la posicion local se quedaria vacio.
    auto frame_a = demo::panel("A", guayaba::Vec2{1, 0}, guayaba::Vec2{panel_w, 3},
                               guayaba::Color::BLACK, guayaba::Color::BRIGHT_GREEN);
    guayaba::NodeRef<guayaba::Node2D> frame_a_ref = make_node_ref<guayaba::Node2D>(frame_a.get());
    panel_ref->add_child(std::move(frame_a));

    auto frame_b = demo::panel("B", guayaba::Vec2{panel_w + 2, 0}, guayaba::Vec2{panel_b_w, 3},
                               guayaba::Color::BLACK, guayaba::Color::BRIGHT_YELLOW);
    guayaba::NodeRef<guayaba::Node2D> frame_b_ref = make_node_ref<guayaba::Node2D>(frame_b.get());
    panel_ref->add_child(std::move(frame_b));

    // El cuadro de color que se ve mudarse de sitio en sitio. Su handle es
    // NodeRef<NodeBox> porque el nodo es un NodeBox, y asi el resto de la escena
    // lo usa con su tipo (get_size, change_background_color) sin castear.
    // Nace dentro de Panel, y su posicion cae dentro del marco A: cuando se
    // muda, el global se queda y la local cambia.
    auto rojo_node = std::make_unique<guayaba::NodeBox>("Rojo", guayaba::Vec2{2, 1},
                                                       guayaba::Vec2{4, 1},
                                                       guayaba::Color::BRIGHT_RED);
    guayaba::NodeRef<guayaba::NodeBox> rojo_ref =
        make_node_ref<guayaba::NodeBox>(rojo_node.get());
    rojo_ref->attach_script<tree_lab_detail::Lifewatch>("Rojo", log_ref);
    panel_ref->add_child(std::move(rojo_node));

    auto label_a = demo::text_line("label_a", guayaba::Vec2{0, 0},
                                   tree_lab_detail::fit(wide ? "marco A" : "A", mark_width),
                                   guayaba::Color::BRIGHT_GREEN);
    frame_a_ref->add_child(std::move(label_a));

    auto label_b = demo::text_line("label_b", guayaba::Vec2{0, 0},
                                   tree_lab_detail::fit(wide ? "marco B" : "B", mark_width),
                                   guayaba::Color::BRIGHT_YELLOW);
    frame_b_ref->add_child(std::move(label_b));

    auto mark_a_node = demo::text_line("mark_a", guayaba::Vec2{0, 2}, "  -  ",
                                       guayaba::Color::BRIGHT_WHITE);
    guayaba::NodeRef<guayaba::NodeUI> mark_a_ref =
        make_node_ref<guayaba::NodeUI>(mark_a_node.get());
    frame_a_ref->add_child(std::move(mark_a_node));

    auto mark_b_node = demo::text_line("mark_b", guayaba::Vec2{0, 2}, "  -  ",
                                       guayaba::Color::BRIGHT_WHITE);
    guayaba::NodeRef<guayaba::NodeUI> mark_b_ref =
        make_node_ref<guayaba::NodeUI>(mark_b_node.get());
    frame_b_ref->add_child(std::move(mark_b_node));

    // Las tres lineas de estado del reparent. Las repinta el script, asi que
    // nacen con algo legible y quedan correctas en el primer frame.
    auto req_node = demo::text_line("req", guayaba::Vec2{lx, oy + 2}, "idle",
                                    guayaba::Color::BRIGHT_YELLOW);
    guayaba::NodeRef<guayaba::NodeUI> req_ref = make_node_ref<guayaba::NodeUI>(req_node.get());
    scene_ref->add_child(std::move(req_node));

    auto parent_node = demo::text_line("parent", guayaba::Vec2{lx, oy + 3}, "padre:Panel",
                                       guayaba::Color::BRIGHT_WHITE);
    guayaba::NodeRef<guayaba::NodeUI> parent_ref =
        make_node_ref<guayaba::NodeUI>(parent_node.get());
    scene_ref->add_child(std::move(parent_node));

    auto pos_node = demo::text_line("pos", guayaba::Vec2{lx, oy + 4}, "g- l-",
                                    guayaba::Color::BRIGHT_MAGENTA);
    guayaba::NodeRef<guayaba::NodeUI> pos_ref = make_node_ref<guayaba::NodeUI>(pos_node.get());
    scene_ref->add_child(std::move(pos_node));

    // El nodo que lleva la logica de la seccion 1. No dibuja nada.
    auto driver_node = std::make_unique<guayaba::Node2D>("rope", guayaba::Vec2{0, 0});
    guayaba::NodeRef<guayaba::Node2D> driver_ref =
        make_node_ref<guayaba::Node2D>(driver_node.get());
    scene_ref->add_child(std::move(driver_node));

    auto rope = driver_ref->attach_script<tree_lab_detail::Rope>(
        scene_ref, rojo_ref, panel_ref, frame_a_ref, frame_b_ref, log_ref);
    rope->wide = wide;
    rope->path_width = path_width;
    rope->mark_width = mark_width;
    rope->mark_a = mark_a_ref;
    rope->mark_b = mark_b_ref;
    rope->line_req = req_ref;
    rope->line_parent = parent_ref;
    rope->line_pos = pos_ref;

    // -- seccion 3: el subarbol que se libera y se rehace ----------------------
    // El "Rail" es un Node2D invisible que existe para que el h_slider tenga un
    // contenedor que empujar: el prefab sobrescribe su eje, asi que empieza en
    // x = 0 y el tirador lo lleva hacia la izquierda.
    auto rail_node = std::make_unique<guayaba::Node2D>("Rail", guayaba::Vec2{0, oy + 15});
    guayaba::NodeRef<guayaba::Node2D> rail_ref = make_node_ref<guayaba::Node2D>(rail_node.get());
    scene_ref->add_child(std::move(rail_node));

    auto stage_node = demo::panel("Escenario", guayaba::Vec2{lx, 0},
                                  guayaba::Vec2{left_w, 1}, guayaba::Color::BLACK,
                                  guayaba::Color::BRIGHT_CYAN);
    guayaba::NodeRef<guayaba::Node2D> stage_ref = make_node_ref<guayaba::Node2D>(stage_node.get());
    rail_ref->add_child(std::move(stage_node));

    auto tag_node = demo::text_line("tag", guayaba::Vec2{left_w - 5, 0}, "n=0",
                                    guayaba::Color::BRIGHT_CYAN);
    guayaba::NodeRef<guayaba::NodeUI> tag_ref = make_node_ref<guayaba::NodeUI>(tag_node.get());
    stage_ref->add_child(std::move(tag_node));

    auto ops_node = std::make_unique<guayaba::Node2D>("ops", guayaba::Vec2{0, 0});
    guayaba::NodeRef<guayaba::Node2D> ops_ref = make_node_ref<guayaba::Node2D>(ops_node.get());
    scene_ref->add_child(std::move(ops_node));

    auto ops = ops_ref->attach_script<tree_lab_detail::SubtreeOps>(rail_ref, log_ref);
    ops->text_width = left_w;
    ops->line_tag = tag_ref;

    auto count_one_node = demo::text_line("count1", guayaba::Vec2{lx, oy + 10}, "hijos 0 total 0",
                                          guayaba::Color::BRIGHT_WHITE);
    guayaba::NodeRef<guayaba::NodeUI> count_one_ref =
        make_node_ref<guayaba::NodeUI>(count_one_node.get());
    scene_ref->add_child(std::move(count_one_node));
    ops->line_one = count_one_ref;

    auto count_two_node = demo::text_line("count2", guayaba::Vec2{lx, oy + 11}, "viejo 0 sq -",
                                          guayaba::Color::BRIGHT_WHITE);
    guayaba::NodeRef<guayaba::NodeUI> count_two_ref =
        make_node_ref<guayaba::NodeUI>(count_two_node.get());
    scene_ref->add_child(std::move(count_two_node));
    ops->line_two = count_two_ref;

    // El subarbol de arranque, montado por el mismo camino que "recrear": asi
    // el primer frame ya enseña el estado bueno.
    ops->recreate();

    // -- seccion 4: las rutas --------------------------------------------------
    auto paths_node = demo::text_block("paths", guayaba::Vec2{right_x, oy + 1},
                                       {"Panel/Rojo -> -", "Panel/A/Rojo -> -",
                                        "Panel/B/Rojo -> -", "as<SQ> Panel/A -> -",
                                        "Panel/Fantasma ->", "  no encontrado"},
                                       guayaba::Color::BRIGHT_GREEN);
    guayaba::NodeRef<guayaba::NodeUI> paths_ref =
        make_node_ref<guayaba::NodeUI>(paths_node.get());
    scene_ref->add_child(std::move(paths_node));
    rope->line_paths = paths_ref;

    // No hay slider ni dialogo. La otra linea de trabajo traia una libreria de
    // prefabricados de interfaz -include/guayaba/UI/UIPrefabs.hpp, con
    // pui::h_slider() y pui::confirm_dialog()-, y este motor no la tiene. Es
    // codigo del MOTOR, no de la demo, asi que no se porta: reproducir un slider
    // y un dialogo arrastrable aqui seria escribir una libreria de UI dentro de un
    // ejemplo para que el ejemplo cuadre.
    //
    // Lo que queda de esta escena es lo que va del arbol: cambiar de padre, la
    // liberacion en cascada, los handles que caducan y las rutas que se查找 a
    // proposito. El subarbol se sigue moviendo, con los botones de reparent.

    // -- cabeceras ------------------------------------------------------------
    scene_ref->add_child(demo::text_line(
        "head", guayaba::Vec2{lx, oy},
        tree_lab_detail::fit(wide ? "TREE LAB: ESTRUCTURA DIFERIDA" : "TREE LAB", left_w),
        guayaba::Color::BRIGHT_CYAN));

    scene_ref->add_child(demo::text_line(
        "head1", guayaba::Vec2{lx, oy + 1},
        tree_lab_detail::fit(wide ? "1 REPARENT: el clic pide, el frame aplica"
                                  : "1 REPARENT",
                             left_w),
        guayaba::Color::BRIGHT_CYAN));

    scene_ref->add_child(demo::text_line(
        "head2", guayaba::Vec2{lx, oy + 9},
        tree_lab_detail::fit(wide ? "2 SUBTREE: queue_free en cascada e idempotente"
                                  : "2 SUBTREE",
                             left_w),
        guayaba::Color::BRIGHT_CYAN));

    // En columna estrecha la fila 14 la ocupa el boton "recrear", asi que la
    // pista solo se pinta cuando hay sitio.
    if (wide) {
        scene_ref->add_child(demo::text_line(
            "hint", guayaba::Vec2{lx, oy + 14},
            tree_lab_detail::fit("orden del frame: update, draw y luego borrar", left_w),
            // Color.hpp no declara constante para BRIGHT_BLACK, pero from_name si
            // la conoce; main.cpp y nav.cpp pasan el mismo nombre.
            "BRIGHT_BLACK"));
    }

    scene_ref->add_child(demo::text_line(
        "head3", guayaba::Vec2{right_x, oy},
        tree_lab_detail::fit("3 RUTAS: get_node / get_node_or_null", right_w),
        guayaba::Color::BRIGHT_CYAN));

    scene_ref->add_child(demo::text_line(
        "log_head", guayaba::Vec2{right_x, oy + 8},
        tree_lab_detail::fit("LOG DE HOOKS (ultimas 12)", right_w),
        guayaba::Color::BRIGHT_YELLOW));

    // -- botones --------------------------------------------------------------
    // Cada uno captura un handle y nada mas: el del script, no el del nodo, asi
    // que si ese script se suelta el clic deja de hacer algo en vez de lanzar.

    const guayaba::NodeRef<guayaba::Node2D> rope_handle =
        make_node_ref<guayaba::Node2D>(rope.node());
    const guayaba::NodeRef<guayaba::Node2D> ops_handle =
        make_node_ref<guayaba::Node2D>(ops.node());

    scene_ref->add_child(demo::action_button("to_a", guayaba::Vec2{lx, oy + 5}, "-> A",
                                             [rope_handle] {
                                                 if (tree_lab_detail::Rope* r = script_ptr<tree_lab_detail::Rope>(rope_handle)) r->request_a();
                                             }));
    scene_ref->add_child(demo::action_button("to_b", guayaba::Vec2{lx + 7, oy + 5}, "-> B",
                                             [rope_handle] {
                                                 if (tree_lab_detail::Rope* r = script_ptr<tree_lab_detail::Rope>(rope_handle)) r->request_b();
                                             }));

    if (wide) {
        scene_ref->add_child(demo::action_button("liberar", guayaba::Vec2{lx, oy + 12},
                                                 "liberar", [ops_handle] {
                                                     if (tree_lab_detail::SubtreeOps* r = script_ptr<tree_lab_detail::SubtreeOps>(ops_handle)) r->release();
                                                 }));
        scene_ref->add_child(demo::action_button("liberar2", guayaba::Vec2{lx + 10, oy + 12},
                                                 "liberar x2", [ops_handle] {
                                                     if (tree_lab_detail::SubtreeOps* r = script_ptr<tree_lab_detail::SubtreeOps>(ops_handle)) r->release_twice();
                                                 }));
        scene_ref->add_child(demo::action_button("recrear", guayaba::Vec2{lx, oy + 13}, "recrear",
                                                 [ops_handle] {
                                                     if (tree_lab_detail::SubtreeOps* r = script_ptr<tree_lab_detail::SubtreeOps>(ops_handle)) r->recreate();
                                                 }));
    } else {
        scene_ref->add_child(demo::action_button("liberar", guayaba::Vec2{lx, oy + 12},
                                                 "liberar", [ops_handle] {
                                                     if (tree_lab_detail::SubtreeOps* r = script_ptr<tree_lab_detail::SubtreeOps>(ops_handle)) r->release();
                                                 }));
        scene_ref->add_child(demo::action_button("liberar2", guayaba::Vec2{lx, oy + 13},
                                                 "liberar x2", [ops_handle] {
                                                     if (tree_lab_detail::SubtreeOps* r = script_ptr<tree_lab_detail::SubtreeOps>(ops_handle)) r->release_twice();
                                                 }));
        scene_ref->add_child(demo::action_button("recrear", guayaba::Vec2{lx, oy + 14}, "recrear",
                                                 [ops_handle] {
                                                     if (tree_lab_detail::SubtreeOps* r = script_ptr<tree_lab_detail::SubtreeOps>(ops_handle)) r->recreate();
                                                 }));
    }

    return scene;
}

} // namespace demo

#endif // GUAYABA_EXAMPLES_SCENES_TREE_LAB_HPP