#ifndef GUAYABA_EXAMPLES_SCENES_CYCLES_LAB_HPP
#define GUAYABA_EXAMPLES_SCENES_CYCLES_LAB_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include <guayaba/Guayaba.hpp>

#include "nav.hpp"

/*
    Cycles Lab: el defecto que motivo el paso de shared_ptr a unique_ptr + NodeRef,
    hecho visible y comprobable con el raton por encima.

    El bucle antiguo era este:

        dialogo -> hijos -> boton -> lambda -> dialogo

    El handler de un hijo capturaba el dialogo POR VALOR, y el dialogo era dueno del
    hijo: nadie soltaba a nadie. El subarbol entero se quedaba registrado para
    siempre, y lo unico que avisaba era el contador de nodos vivos, que subia y no
    voltaba a bajar. Esa fuga es la de test_no_reference_cycle de api_test.cpp, hecha
    aqui una escena entera con la que se juega.

    Lo que ve el humano, de arriba abajo:

      1 REGISTRO   La linea grande es NodeRegistry::instance().live_count(), leida
                   en vivo. "antes" se mide en el PRIMER frame, cuando la escena ya
                   esta construida y el subarbol todavia no existe: ese es el
                   punto de partida honesto. Mientras la caja existe sale
                   "registro: antes=12 ahora=17 (+5)", y el +5 sale de CONTAR el
                   subarbol, no de escribirlo a mano.
                   Tras "liberar" la misma linea dice
                   "registro: antes=12 ahora=12 (se libero todo)", en verde. Esa
                   vuelta exacta al punto de partida es TODO el argumento: con
                   shared_ptr en la lambda, el numero se habria quedado en 17 para
                   siempre.

      2 CAJA VIVA  La caja es un subarbol de 5 nodos: un NodeSQ, su rotulo, dos
                   NodeButton y un NodeSQ anidado. Las cuatro flechas de sus
                   handlers son NodeRef y nada mas. El boton "hijo" se refiere a
                   SU PROPIO PADRE (el panel) y le cambia el color: el caso exacto
                   del bug, escrito bien. El "nieto", que esta dos niveles por
                   debajo, llega al mismo panel por handle y le mueve una celda.
                   Ninguno de los dos posee al padre, y por eso "liberar" lo baja
                   entero.
                   El rotulo de arriba lleva la cuenta de clics y el fondo de la
                   caja tambien suma: ese clic no es el de un boton, lo lee el
                   driver con el raton y decide por geometria (los botones quedan
                   fuera, para no contar dos veces lo mismo).

      3 HANDLES    Las dos lineas que hay bajo el panel son el parte de
                   auditoria. Mientras la caja vive, el handle del panel y el del
                   script dan get() y get_script_as<>() no nulos. Tras
                   liberarla siguen escribiendose en pantalla, y dicen valid=0 y
                   try=null: el motor no revienta, avisa. Esa es la respuesta
                   buena a un handle colgando, y se ve sin leer un backtrace.

      4 CONTADOR  Una barra y un recuento de frames que se mueven solos: la escena
                   esta viva cuando no se pulsa nada. Los lleva el propio script
                   del driver. Al lado, la ficha lleva su propio guion, que se mueve
                   desde on_process y escribe la posicion en su owner.

    Los botones: "crear" monta el subarbol, "liberar" llama a queue_free() sobre el
    nodo que lo contiene ("lib x2" repite la llamada para teachar que queue_free es
    idempotente), y "attach" le pone un guion a la ficha.

    LO QUE NO ESTA, Y POR QUE
    "detach" y su pareja "swap" no se han portado. Este motor no puede soltar ni
    cambiar el guion de un nodo vivo: attach_script() sella el hueco en el primer
    attach y no hay desenlace. La razon esta escrita en Core/ScriptRef.hpp - si un
    nodo pudiera perder su guion mientras vive, todo ScriptRef emitido se quedaria
    colgando -, y la escena que las enseñaba es justo la que demuestra por que no.

    Tampoco hay flush_pending(), asi que "liberar" deja el borrado para el final
    del frame del bucle. El efecto en pantalla es el mismo y mejor: este frame los
    handles siguen vivos y el subarbol se sigue viendo, y es al siguiente cuando
    cae.

    Solo el raton. Casi todos los clics pasan por NodeButton, que ya sabe mirar
    mouse_pos y l_click_just_pressed; el unico punto donde esta escena lee Input
    por su cuenta es el fondo de la caja, que se puede pulsar sin coger ningun
    boton, y ese punto va con su guardia de g_terminal: sin terminal no se pregunta.
    Sin terminal la escena se dibuja igual y no hace nada.

    La construccion usa unique_ptr + add_child (quien construye es dueno de lo que
    crea) y make_node_ref para pasar a handles en cuanto existe el nodo. La escena
    no sobreescribe update ni draw en ningun sitio, asi que no tiene que delegar
    en la base.

    Los textos son ASCII y se recortan al ancho de su columna, porque el motor no
    envuelve lineas: un recorte con "..." informa mas que desbordarse encima del
    panel de al lado.
*/
namespace demo {

namespace cycles_lab_detail {

// -- texto ------------------------------------------------------------------

// Recorta a width, marcando el recorte. La columna manda: el motor no envuelve.
inline std::string fit(const std::string& text, int width) {
    if (width < 1) return std::string();
    if (static_cast<int>(text.size()) <= width) return text;
    if (width <= 3) return text.substr(0, static_cast<size_t>(width));
    return text.substr(0, static_cast<size_t>(width - 3)) + "...";
}

inline std::string num(int value) { return std::to_string(value); }

inline std::string onoff(bool value) { return value ? "1" : "0"; }

// La frase entera si cabe en la columna, la corta si no. Un texto que se corta a
// media palabra deja de decir lo que dice, que es justo lo que importa aqui.
inline std::string pick(const std::string& full, const std::string& short_text, int width) {
    return static_cast<int>(full.size()) <= width ? full : short_text;
}

// -- el script del boton de dentro de la caja --------------------------------

/*
    El contador de clics vive aqui, en un script, y no en un int suelto capturado
    por la lambda. El handler recibe el handle TIPADO de este script, asi que el
    estado es de verdad del boton, y el handle caduca con el si el subarbol se va.

    ticks se lleva solo mientras el boton esta en el arbol, asi que tambien cuenta
    eso.
*/
class HitTally : public guayaba::Script<guayaba::NodeButton> {
public:
    int hits = 0;
    int ticks = 0;
    int shift = 0;   // la celda extra que el nieto le pide al panel: 0 o 1

    void on_process(double) override { ++ticks; }

    void bump(int times) {
        for (int i = 0; i < times; ++i) ++hits;
    }
};

// -- el script que mueve la ficha --------------------------------------------

/*
    La ficha es un NodeBox y no se mueve solo: la mueve ESTE script, en
    on_process, escribiendo en la posicion de su owner. El owner se pide por handle
    con get_owner_as, que en codigo por frame va vacio en vez de lanzar.

    Consecuencia buscada: el estado del movimiento es del script y no del nodo. La
    ficha se para cuando su guion se va con ella, y eso ya no se puede provocar
    desde un boton en este motor, asi que la ficha se queda en su sitio hasta que
    el nodo muere entero.
*/
class Pulse : public guayaba::Script<guayaba::Node2D> {
public:
    int span = 8;      // celdas de recorrido
    int steps = 0;     // frames vividos, solo legibles mientras el script viva
    double phase = 0.0;

    void on_process(double delta_time) override {
        ++steps;
        guayaba::Node2D* self = get_owner_as<guayaba::Node2D>();
        if (!self || span < 1) return;
        phase += static_cast<double>(delta_time) * 2.0;
        const double half = (std::sin(phase) + 1.0) * 0.5;
        self->set_local_position({1 + static_cast<int>(half * static_cast<double>(span)), 1});
    }
};

// -- el script que lleva la escena -------------------------------------------

/*
    Un solo script lee el registro y repinta las siete lineas. Es el nodo que tiene
    todo: los handles a los textos que escribe, el banco donde vive la caja, la
    ficha, y los tres handles que se guardan a PROPOSITO para verlos caducar
    (caja, tally, pulse).

    Ni un solo puntero crudo en los miembros: lo que hay son handles, que ocupan 8 bytes
    de nodo (ranura mas generacion), no poseen nada y por eso el subarbol se puede
    soltar sin que nada tire de el.
*/
class CycleWatch : public guayaba::Script<guayaba::Node2D> {
public:
    // -- handles hacia la escena --
    guayaba::NodeRef<guayaba::NodeUI> line_registry;
    guayaba::NodeRef<guayaba::NodeUI> line_shape;
    guayaba::NodeRef<guayaba::NodeUI> line_panel;
    guayaba::NodeRef<guayaba::NodeUI> line_script;
    guayaba::NodeRef<guayaba::NodeUI> line_counter;
    guayaba::NodeRef<guayaba::NodeUI> line_bar;
    guayaba::NodeRef<guayaba::NodeUI> line_token;
    guayaba::NodeRef<guayaba::Node2D> bench;    // "Banco": el padre del subarbol
    guayaba::NodeRef<guayaba::Node2D> ficha;    // la ficha que mueve Pulse

    // -- estado medido --
    int before = -1;       // el registro en el primer frame: el punto de partida
    int frames = 0;
    int generation = 0;
    int bay = 17;          // ancho de la caja y del carril
    int span = 8;          // recorrido de la ficha
    int bar_cells = 8;
    int full_w = 38;       // ancho util de las lineas de arriba
    int left_w = 20;       // columna izquierda
    int right_w = 17;      // columna derecha
    bool ever_released = false;
    bool marked = false;   // hay una peticion de liberate sin resolver
    int freed_nodes = 0;
    int freed_kids = 0;

    // -- los tres handles que se dejan caducar a proposito --
    guayaba::NodeRef<guayaba::NodeSQ> caja;
    guayaba::NodeRef<guayaba::Node> tally;
    guayaba::NodeRef<guayaba::Node> pulse;

    void on_process(double) override {
        ++frames;
        // La medicion sale en el PRIMER frame y no antes: la escena entera ya esta
        // construida y el subarbol todavia no existe, que es justo lo que hace
        // falta para que "antes" sea comparable con "despues".
        if (before < 0) before = static_cast<int>(guayaba::detail::NodeRegistry::instance().live_count());
        click_panel();
        paint();
    }

    // -- los tres botones de la izquierda ----------------------------------

    void create() {
        guayaba::Node2D* host = bench.get();
        if (!host || caja.is_valid()) return;   // ya hay caja: no se duplica
        ++generation;
        host->add_child(build_caja());
        ever_released = false;
        marked = false;
        paint();
    }

    void release() {
        guayaba::Node2D* host = bench.get();
        guayaba::NodeSQ* live = caja.get();
        if (!host || !live) return;

        // Los handles NO se limpian: son la prueba. Se quedan apuntando al panel y
        // a su script, y en pantalla se vera como pasan de v=1 a v=0.
        freed_nodes = count_nodes(guayaba::NodeRef<guayaba::Node>(caja.get()));
        freed_kids = static_cast<int>(live->get_child_count());
        marked = true;

        // queue_free() solo marca. Quien destruye de verdad es el bucle de frames,
        // al final del frame, asi que este mismo frame los handles siguen vivos y
        // el subarbol sigue dibujandose: es el siguiente frame donde se ve caer.
        //
        // El motor anterior tenia flush_pending() para forzar la destruccion desde
        // aqui y asi ver el efecto en el acto. Ese metodo no existe en este, y no
        // se ha anadido: el borrado al final del frame es lo que garantiza que
        // ningun nodo desaparezca bajo el update o el draw que lo esta recorriendo.
        live->queue_free();
        paint();
    }

    void release_twice() {
        release();
        release();          // queue_free es idempotente: la segunda no cambia nada
        paint();
    }

    // -- el boton de la derecha ---------------------------------------------

    void attach_pulse() {
        guayaba::Node2D* node = ficha.get();
        if (!node) return;
        // El handle del nodo y el del guion son dos variables distintas en este
        // motor: antes el handle sabia que guion llevaba y se guardaba el mismo
        // para las dos cosas.
        pulse = make_node_ref<guayaba::Node2D>(node);
        if (Pulse* p = script_ptr<Pulse>(pulse)) p->span = span;
        paint();
    }

private:
    // El numero de clics del boton, o "n/a" si la caja ya no esta. El guion vive
    // en el nodo, asi que se llega a el a traves del handle del nodo.
    std::string clics_del_tally() const {
        const cycles_lab_detail::HitTally* t = script_ptr<cycles_lab_detail::HitTally>(tally);
        return t ? num(t->hits) : std::string("n/a");
    }

    // Recuento recursivo andando con handles, no con punteros: cada get_child(i)
    // devuelve un handle que caduca solo si el nodo ha muerto.
    static int count_nodes(const guayaba::NodeRef<guayaba::Node>& node, int depth = 0) {
        if (!node.is_valid() || depth > 12) return 0;
        int total = 1;
        const int kids = static_cast<int>(node->get_child_count());
        for (int i = 0; i < kids; ++i) {
            total += count_nodes(node->get_child(static_cast<size_t>(i)), depth + 1);
        }
        return total;
    }

    static void say(const guayaba::NodeRef<guayaba::NodeUI>& target, const std::string& line,
                    const std::string& color) {
        guayaba::NodeUI* ui = target.get();
        if (!ui) return;
        ui->set_text({line});
        ui->change_text_color(color);
    }

    /*
        El fondo de la caja tambien cuenta, y ese clic NO es el de un boton: se lee
        aqui, con el raton, y se decide por geometria.

        Por eso el unico punto donde esta escena toca Input es este, y por eso va
        con guardia: sin terminal no hay g_terminal que preguntar y no hay raton que
        leer. Los botones quedan fuera del rectangulo, para que pulsarlos cuente una
        sola vez (la suya, que va por su propio on_click).
    */
    void click_panel() {
        // Sin terminal no hay g_terminal que preguntar ni raton que leer. Este es
        // el UNICO punto de la escena que toca Input, y por eso es el que lleva la
        // guardia.
        if (!guayaba::Input::g_terminal || !guayaba::Input::l_click_just_pressed) return;
        if (!tally.is_valid()) return;
        guayaba::NodeSQ* panel = caja.get();
        if (!panel) return;

        // El panel es un NodeSQ y los NodeSQ no tienen is_inside (solo lo tiene
        // NodeUI), asi que el rectangulo se compara a mano con su posicion global
        // y su tamano.
        const guayaba::Vec2 origin = panel->get_global_position();
        const guayaba::Vec2 extent = panel->get_size();
        const guayaba::Vec2 mouse = guayaba::Input::mouse_pos;
        if (mouse.x < origin.x || mouse.x >= origin.x + extent.x) return;
        if (mouse.y < origin.y || mouse.y >= origin.y + extent.y) return;

        // Los botones tienen su propio clic: aqui solo cuenta el fondo, o el mismo
        // pulsado se contaria dos veces.
        if (inside(caja->get_node<guayaba::NodeUI>("hijo"), mouse)) return;
        if (inside(caja->get_node<guayaba::NodeUI>("anidado/nieto"), mouse)) return;
        if (HitTally* t = script_ptr<HitTally>(tally)) t->bump(1);
    }

    // Hit test por handles: si el nodo no esta, no hay nada que pulsar.
    static bool inside(const guayaba::NodeRef<guayaba::NodeUI>& target, const guayaba::Vec2& point) {
        guayaba::NodeUI* ui = target.get();
        return ui != nullptr && ui->is_inside(point);
    }

    // -- el subarbol: 5 nodos y 4 capturas, todas NodeRef -------------------

    /*
        Construido como lo construye el resto de la demo: unique_ptr mientras el
        codigo es dueno, make_node_ref en cuanto el nodo existe, add_child para
        entregar la propiedad, y handles a partir de ahi.

        Los dos handlers son el bug antiguo escrito bien: el handler de un hijo se
        refiere a su propio padre. Con shared_ptr eso era
        panel -> hijos -> boton -> lambda -> panel y nadie lo soltaba nunca. Con
        NodeRef la flecha va al reves, no se puede cerrar y el panel se puede
        soltar entero.
    */
    std::unique_ptr<guayaba::NodeSQ> build_caja() {
        auto panel_node = std::make_unique<guayaba::NodeSQ>(
            "Caja", guayaba::Vec2{0, 0}, guayaba::Vec2{bay, 5},
            guayaba::Color::BRIGHT_CYAN, guayaba::Color::BLACK);
        guayaba::NodeRef<guayaba::NodeSQ> panel_ref =
            make_node_ref<guayaba::NodeSQ>(panel_node.get());

        // El rotulo va en la fila de arriba, y su ancho maximo es el hueco que
        // queda hasta el cuadro anidado: el motor no envuelve, asi que un texto
        // largo se comeria el marco de al lado.
        auto label_node = std::make_unique<guayaba::NodeUI>(
            "rotulo", guayaba::Vec2{1, 1}, guayaba::Color::BRIGHT_WHITE,
            guayaba::Color::BLACK, std::vector<std::string>{"viva 0"});
        panel_ref->add_child(std::move(label_node));

        // El boton de en medio: su handler cambia el color del PANEL, o sea, se
        // refiere a su propio padre por handle. Dos capturas: el panel y el
        // script tipado de este boton. Ninguna posee nada.
        auto child_node = std::make_unique<guayaba::NodeButton>(
            "hijo", guayaba::Vec2{1, 2}, guayaba::Color::BLACK, guayaba::Color::BRIGHT_WHITE,
            std::vector<std::string>{" hijo "});
        guayaba::NodeRef<guayaba::NodeButton> child_ref =
            make_node_ref<guayaba::NodeButton>(child_node.get());
        const guayaba::NodeRef<guayaba::NodeButton> tally_ref = child_ref;

        child_ref->connect_signal<>("clicked", [panel_ref, tally_ref] {
            if (HitTally* t = script_ptr<HitTally>(tally_ref)) t->bump(1);
            // try_get_node, no get_node: un handler corre dentro del update de otro
            // nodo y aqui no se quiere una excepcion ni aunque el panel se muera
            // entre el clic y este momento.
            if (guayaba::NodeSQ* panel = panel_ref.get()) {
                const HitTally* t = script_ptr<HitTally>(tally_ref);
                const bool odd = t != nullptr && t->hits % 2 != 0;
                panel->change_background_color(odd ? guayaba::Color::BRIGHT_BLUE
                                                    : guayaba::Color::BLACK);
            }
        });
        panel_ref->add_child(std::move(child_node));

        // El nieto: dos niveles por debajo del panel, y tambien lo alcanza por
        // handle para moverlo una celda. El desplazamiento alterno lo decide el
        // script, que es donde vive ese estado.
        auto nested_node = std::make_unique<guayaba::NodeSQ>(
            "anidado", guayaba::Vec2{bay - 8, 1}, guayaba::Vec2{7, 3},
            guayaba::Color::BRIGHT_YELLOW, guayaba::Color::BLACK);
        guayaba::NodeRef<guayaba::NodeSQ> nested_ref =
            make_node_ref<guayaba::NodeSQ>(nested_node.get());

        auto grand_node = std::make_unique<guayaba::NodeButton>(
            "nieto", guayaba::Vec2{1, 1}, guayaba::Color::BLACK, guayaba::Color::BRIGHT_YELLOW,
            std::vector<std::string>{" +10 "});
        guayaba::NodeRef<guayaba::NodeButton> grand_ref =
            make_node_ref<guayaba::NodeButton>(grand_node.get());

        grand_ref->connect_signal<>("clicked", [panel_ref, tally_ref] {
            if (HitTally* t = script_ptr<HitTally>(tally_ref)) {
                t->shift = t->shift ? 0 : 1;
                t->bump(10);
            }
            if (guayaba::NodeSQ* panel = panel_ref.get()) {
                const HitTally* t = script_ptr<HitTally>(tally_ref);
                const int shift = t != nullptr ? t->shift : 0;
                panel->set_local_position({shift, 0});
            }
        });
        nested_ref->add_child(std::move(grand_node));
        panel_ref->add_child(std::move(nested_node));

        // Los handles oficiales se toman aqui, mientras el codigo es dueno del
        // panel. A partir de aqui la escena solo habla con handles, y estos dos no
        // se limpian nunca: liberada la caja, son la prueba de que caducaron.
        caja = panel_ref;
        tally = tally_ref;
        return panel_node;
    }

    // -- lo que se ve -------------------------------------------------------

    void paint() {
        if (before < 0) return;   // todavia no hay nada que comparar

        const bool alive = caja.is_valid();
        // El flush ya corrio: la caja ya no esta, y este es el momento de decirlo.
        if (!alive && marked) {
            marked = false;
            ever_released = true;
        }

        const int now = static_cast<int>(guayaba::detail::NodeRegistry::instance().live_count());
        const int delta = now - before;
        const int nodes = alive ? count_nodes(caja) : 0;
        const int kids = alive ? static_cast<int>(caja->get_child_count()) : 0;

        // La linea grande del registro: una de tres lecturas del mismo numero.
        if (alive) {
            const std::string tail = " (+" + num(delta) + ")" + (marked ? " pedido" : "");
            say(line_registry,
                pick("registro: antes=" + num(before) + " ahora=" + num(now) + tail,
                     "registro: " + num(before) + " -> " + num(now) + tail, full_w),
                guayaba::Color::BRIGHT_CYAN);
            say(line_shape,
                fit("caja: " + num(nodes) + " nodos, " + num(kids) + " hijos, sq ok #"
                        + num(generation),
                    full_w),
                guayaba::Color::BRIGHT_WHITE);
        } else if (ever_released) {
            // EL remate: el registro ha vuelto al punto exacto de partida. Con
            // shared_ptr en la lambda, "ahora" se habria quedado en +5 para siempre.
            say(line_registry,
                pick("registro: antes=" + num(before) + " ahora=" + num(now)
                         + " (se libero todo)",
                     "registro: " + num(before) + " = " + num(now) + " (se libero todo)",
                     full_w),
                guayaba::Color::BRIGHT_GREEN);
            say(line_shape,
                fit("fuera: " + num(freed_nodes) + " nodos, " + num(freed_kids)
                        + " hijos, handles v=0",
                    full_w),
                guayaba::Color::BRIGHT_GREEN);
        } else {
            say(line_registry,
                pick("registro: antes=" + num(before) + " ahora=" + num(now)
                         + " (nada construido)",
                     "registro: " + num(before) + " = " + num(now) + " (nada construido)",
                     full_w),
                guayaba::Color::BRIGHT_CYAN);
            say(line_shape, fit("sin caja todavia: pulsa crear", full_w),
                guayaba::Color::BRIGHT_CYAN);
        }

        // El parte de los handles. get() y get_script_as<>() no lanzan nunca:
        // esto se repinta cada frame, con la caja viva y con la caja muerta.
        if (!alive && !ever_released) {
            // Color.hpp no declara constante para BRIGHT_BLACK, pero from_name si
            // la conoce.
            say(line_panel, "caja: sin crear", "BRIGHT_BLACK");
            say(line_script, "script: sin crear", "BRIGHT_BLACK");
        } else {
            // Verde cuando ya se libero: que un handle colgando se quede vacio en
            // vez de reventar ES la noticia buena, y se lee de un vistazo.
            const std::string color = ever_released ? guayaba::Color::BRIGHT_GREEN
                                                    : guayaba::Color::BRIGHT_WHITE;
            say(line_panel, "caja v=" + onoff(caja.is_valid()) + " try="
                                + (caja.get() ? "ok" : "null"), color);
            say(line_script, "script v=" + onoff(tally.is_valid()) + " try="
                                 + (script_ptr<cycles_lab_detail::HitTally>(tally) ? "ok" : "null"),
                color);
        }

        // El rotulo de dentro de la caja: lo escribe el driver por handle, asi que
        // ver el numero ahi prueba que el clic llego al subarbol y de vuelta.
        if (alive) {
            const guayaba::NodeRef<guayaba::NodeUI> label =
                caja->get_node<guayaba::NodeUI>("rotulo");
            if (guayaba::NodeUI* ui = label.get()) {
                const cycles_lab_detail::HitTally* t =
                    script_ptr<cycles_lab_detail::HitTally>(tally);
                ui->set_text({fit("viva " + (t ? num(t->hits) : std::string("?")),
                                  bay - 9)});
            }
        }

        // El contador vivo: frames y clics. Los clics salen del script del boton,
        // que puede haber muerto con la caja: entonces el handle va vacio y sale
        // "n/a" en vez de una excepcion.
        say(line_counter,
            "f=" + num(frames) + " clics=" + clics_del_tally(),
            guayaba::Color::BRIGHT_WHITE);

        // La barra: el mismo script, para que la escena se mueva sin que se pulse.
        say(line_bar, bar_line(), guayaba::Color::BRIGHT_GREEN);

        // La ficha: mientras su script siga ligado, el handle es valido y el
        // numero se mueve. Al soltarlo, el handle caduca y la x se clava.
        const guayaba::Node2D* box = ficha.get();
        const int x = box ? box->get_local_position().x : -1;
        if (pulse.is_valid()) {
            say(line_token, pick("ficha v=1 x=" + num(x), "v=1 x=" + num(x), right_w),
                guayaba::Color::BRIGHT_GREEN);
        } else {
            say(line_token, pick("ficha v=0 x=" + num(x) + " fijo", "v=0 x=" + num(x) + " fijo",
                                 right_w),
                guayaba::Color::BRIGHT_RED);
        }
    }

    std::string bar_line() const {
        const int width = bar_cells > 1 ? bar_cells : 1;
        const int filled = (frames / 2) % (width + 1);
        char tail[8];
        std::snprintf(tail, sizeof(tail), "%3d%%", filled * 100 / width);

        std::string line = "viva [";
        line.append(static_cast<size_t>(filled), '#');
        line.append(static_cast<size_t>(width - filled), '.');
        line += "] ";
        line += tail;
        return line;
    }
};

} // namespace cycles_lab_detail

/*
    La escena entera. Mide la pantalla con demo::screen_size() y reparte dos
    columnas: a la izquierda el registro, los handles y el contador; a la derecha
    la caja y la ficha. Las lineas de arriba usan el ancho entero, porque la del
    registro es el remate de la escena y no puede quedarse en medio palabra.

    Con 40x20 (el minimo) cabe JUSTO: 16 filas de contenido y la de botones en la
    ultima. Con una pantalla ancha las columnas se separan y a la derecha de la caja
    aparece el antes/despues del defecto, que solo cabe si hay sitio.

    main.cpp cuelga el contenido 4 filas mas abajo, asi que aqui las coordenadas
    son de escena y el desplazamiento lo suma el motor al pintar.
*/
inline std::unique_ptr<guayaba::Node> build_cycles_lab() {
    const guayaba::Vec2 screen = demo::screen_size({100, 60});
    // main.cpp baja el contenido 4 filas: el area util son screen.y - 4.
    const int rows = std::max(screen.y - 4, 16);
    const int width = std::max(screen.x, 24);
    const int full_w = width - 1;

    // Columna izquierda con margen para las lineas de handles (20 columnas), y
    // derecha con margen para la caja (16). El reparto se hace con std::min en
    // las dos directions para que en 40 columnas no se pisen.
    const int left_w = std::min(std::max(width / 2 - 1, 20), 62);
    const int right_x = std::min(left_w + 2, std::max(width - 16, 12));
    const int right_w = std::max(width - right_x - 1, 12);
    const int bay = std::min(right_w, 26);
    const int span = bay - 5 > 1 ? bay - 5 : 1;

    // Con alto de sobra la escena baja un poco, sin dejar de caber.
    const int oy = rows > 20 ? std::min((rows - 20) / 2, 6) : 0;
    const int lx = 1;

    auto scene = std::make_unique<guayaba::Node2D>("cycles_lab", guayaba::Vec2{0, 0});
    guayaba::NodeRef<guayaba::Node2D> scene_ref = make_node_ref<guayaba::Node2D>(scene.get());

    // -- el que mide y repinta: primero hijo, para que el "antes" se lea antes
    //    de que ningun clic de este frame pueda cambiar el registro -----------
    auto driver_node = std::make_unique<guayaba::Node2D>("ciclo", guayaba::Vec2{0, 0});
    guayaba::NodeRef<guayaba::Node2D> driver_ref =
        make_node_ref<guayaba::Node2D>(driver_node.get());
    scene_ref->add_child(std::move(driver_node));

    // -- las lineas de arriba, a ancho entero -------------------------------
    auto registry_node = demo::text_line("registro", guayaba::Vec2{lx, oy + 3},
                                         "registro: midiendo", guayaba::Color::BRIGHT_CYAN);
    guayaba::NodeRef<guayaba::NodeUI> registry_ref =
        make_node_ref<guayaba::NodeUI>(registry_node.get());
    scene_ref->add_child(std::move(registry_node));

    auto shape_node = demo::text_line("forma", guayaba::Vec2{lx, oy + 4},
                                      "sin caja todavia: pulsa crear", guayaba::Color::BRIGHT_CYAN);
    guayaba::NodeRef<guayaba::NodeUI> shape_ref = make_node_ref<guayaba::NodeUI>(shape_node.get());
    scene_ref->add_child(std::move(shape_node));

    // -- los cuatro textos de las columnas ----------------------------------
    auto panel_line_node = demo::text_line("handle_panel", guayaba::Vec2{lx, oy + 6},
                                           "caja: sin crear", guayaba::Color::BRIGHT_WHITE);
    guayaba::NodeRef<guayaba::NodeUI> panel_line_ref =
        make_node_ref<guayaba::NodeUI>(panel_line_node.get());
    scene_ref->add_child(std::move(panel_line_node));

    auto script_line_node = demo::text_line("handle_script", guayaba::Vec2{lx, oy + 7},
                                            "script: sin crear", guayaba::Color::BRIGHT_WHITE);
    guayaba::NodeRef<guayaba::NodeUI> script_line_ref =
        make_node_ref<guayaba::NodeUI>(script_line_node.get());
    scene_ref->add_child(std::move(script_line_node));

    auto counter_node = demo::text_line("contador", guayaba::Vec2{lx, oy + 11}, "f=0 clics=n/a",
                                        guayaba::Color::BRIGHT_WHITE);
    guayaba::NodeRef<guayaba::NodeUI> counter_ref =
        make_node_ref<guayaba::NodeUI>(counter_node.get());
    scene_ref->add_child(std::move(counter_node));

    auto bar_node = demo::text_line("barra", guayaba::Vec2{lx, oy + 10}, "viva [........]   0%",
                                    guayaba::Color::BRIGHT_GREEN);
    guayaba::NodeRef<guayaba::NodeUI> bar_ref = make_node_ref<guayaba::NodeUI>(bar_node.get());
    scene_ref->add_child(std::move(bar_node));

    auto token_node = demo::text_line("token", guayaba::Vec2{right_x, oy + 11}, "v=0 x=-1 fijo",
                                      guayaba::Color::BRIGHT_GREEN);
    guayaba::NodeRef<guayaba::NodeUI> token_ref = make_node_ref<guayaba::NodeUI>(token_node.get());
    scene_ref->add_child(std::move(token_node));

    // -- el banco: el padre del subarbol, y el nodo al que se le pide el flush -
    auto bench_node = std::make_unique<guayaba::Node2D>("Banco", guayaba::Vec2{right_x, oy + 6});
    guayaba::NodeRef<guayaba::Node2D> bench_ref =
        make_node_ref<guayaba::Node2D>(bench_node.get());
    scene_ref->add_child(std::move(bench_node));

    // -- el carril y la ficha: un NodeBox que solo se mueve si su script mueve --
    auto lane_node = demo::panel("Carril", guayaba::Vec2{right_x, oy + 12},
                                 guayaba::Vec2{bay, 3}, guayaba::Color::BLACK,
                                 guayaba::Color::BRIGHT_GREEN);
    guayaba::NodeRef<guayaba::Node2D> lane_ref =
        make_node_ref<guayaba::Node2D>(lane_node.get());
    scene_ref->add_child(std::move(lane_node));

    auto token_box_node = std::make_unique<guayaba::NodeBox>(
        "Ficha", guayaba::Vec2{1, 1}, guayaba::Vec2{3, 1}, guayaba::Color::BRIGHT_CYAN);
    guayaba::NodeRef<guayaba::Node2D> token_box_ref =
        make_node_ref<guayaba::Node2D>(token_box_node.get());
    lane_ref->add_child(std::move(token_box_node));

    // El script va pegado a la ficha: al soltarlo, la ficha se queda donde esta.
    const guayaba::ScriptRef<guayaba::Node2D, cycles_lab_detail::Pulse> pulse_handle =
        token_box_ref->attach_script<cycles_lab_detail::Pulse>();
    pulse_handle->span = span;

    // -- el driver, con sus handles ya apuntando a sitio --------------------
    const guayaba::ScriptRef<guayaba::Node2D, cycles_lab_detail::CycleWatch> ops =
        driver_ref->attach_script<cycles_lab_detail::CycleWatch>();
    ops->bay = bay;
    ops->span = span;
    ops->bar_cells = std::min(std::max(left_w - 12, 4), 24);
    ops->full_w = full_w;
    ops->left_w = left_w;
    ops->right_w = right_w;
    ops->line_registry = registry_ref;
    ops->line_shape = shape_ref;
    ops->line_panel = panel_line_ref;
    ops->line_script = script_line_ref;
    ops->line_counter = counter_ref;
    ops->line_bar = bar_ref;
    ops->line_token = token_ref;
    ops->bench = bench_ref;
    ops->ficha = token_box_ref;
    ops->pulse = make_node_ref<guayaba::Node2D>(token_box_ref.get());

    // -- cabeceras ----------------------------------------------------------
    // Color.hpp no declara constante para BRIGHT_BLACK, pero from_name si la
    // conoce; el nombre se pasa tal cual.
    scene_ref->add_child(demo::text_line(
        "head", guayaba::Vec2{lx, oy},
        cycles_lab_detail::fit(cycles_lab_detail::pick("CYCLES LAB: SIN CICLOS DE REFERENCIA", "CYCLES LAB",
                                    full_w),
            full_w),
        guayaba::Color::BRIGHT_CYAN));

    scene_ref->add_child(demo::text_line(
        "sub", guayaba::Vec2{lx, oy + 1},
        cycles_lab_detail::fit(cycles_lab_detail::pick(
                "cada callback captura NodeRef: una flecha, no una propiedad", "capturas = NodeRef",
                full_w),
            full_w),
        "BRIGHT_BLACK"));

    scene_ref->add_child(demo::text_line(
        "head1", guayaba::Vec2{lx, oy + 2}, "1 REGISTRO DE NODOS VIVOS",
        guayaba::Color::BRIGHT_CYAN));

    scene_ref->add_child(demo::text_line(
        "head2", guayaba::Vec2{lx, oy + 5},
        cycles_lab_detail::fit(cycles_lab_detail::pick("2 CAJA VIVA (pulsa dentro)   3 HANDLES", "2 CAJA VIVA",
                                    full_w),
            full_w),
        guayaba::Color::BRIGHT_CYAN));

    scene_ref->add_child(demo::text_line(
        "head3", guayaba::Vec2{lx, oy + 9}, "4 CONTADOR (script)", guayaba::Color::BRIGHT_CYAN));

    // La cabecera de la ficha solo si la de la escena no llega hasta ella: en
    // 40 columnas las dos se pisarian en la misma fila.
    if (right_x >= 42) {
        scene_ref->add_child(demo::text_line(
            "head4", guayaba::Vec2{right_x, oy + 0}, cycles_lab_detail::fit("5 FICHA", right_w),
            guayaba::Color::BRIGHT_CYAN));
    }

    // -- el antes/despues, solo si hay ancho a la derecha de la caja ----------
    if (right_w >= 72) {
        const int ex = right_x + bay + 4;
        const int ew = std::max(width - ex - 1, 10);
        scene_ref->add_child(demo::text_line(
            "was1", guayaba::Vec2{ex, oy + 6}, cycles_lab_detail::fit("ANTES: shared_ptr", ew),
            guayaba::Color::BRIGHT_RED));
        scene_ref->add_child(demo::text_line(
            "was2", guayaba::Vec2{ex, oy + 7},
            cycles_lab_detail::fit("caja -> hijos -> boton -> lambda -> caja", ew), guayaba::Color::BRIGHT_RED));
        scene_ref->add_child(demo::text_line(
            "was3", guayaba::Vec2{ex, oy + 8},
            cycles_lab_detail::fit("ciclo cerrado: nadie soltaba a nadie", ew), guayaba::Color::BRIGHT_RED));
        scene_ref->add_child(demo::text_line(
            "now1", guayaba::Vec2{ex, oy + 10}, cycles_lab_detail::fit("AHORA: NodeRef", ew),
            guayaba::Color::BRIGHT_GREEN));
        scene_ref->add_child(demo::text_line(
            "now2", guayaba::Vec2{ex, oy + 11},
            cycles_lab_detail::fit("la lambda no posee nada: solo apunta", ew), guayaba::Color::BRIGHT_GREEN));
        scene_ref->add_child(demo::text_line(
            "now3", guayaba::Vec2{ex, oy + 12},
            cycles_lab_detail::fit("liberar la baja entera, y el handle queda obsoleto", ew),
            guayaba::Color::BRIGHT_GREEN));
        scene_ref->add_child(demo::text_line(
            "now4", guayaba::Vec2{ex, oy + 13},
            cycles_lab_detail::fit("v=0 try=null en vez de un crash", ew), guayaba::Color::BRIGHT_GREEN));
    }

    // -- botones -------------------------------------------------------------
    // Todos capturan el handle TIPADO del driver, no el nodo: si ese script se
    // soltara, valid() lo dice y el clic deja de hacer algo en vez de lanzar.

    scene_ref->add_child(demo::action_button("crear", guayaba::Vec2{lx, oy + 13}, "crear",
                                             [ops] {
                                                 if (ops.is_valid()) ops->create();
                                             }));
    scene_ref->add_child(demo::action_button("liberar", guayaba::Vec2{lx, oy + 14}, "liberar",
                                             [ops] {
                                                 if (ops.is_valid()) ops->release();
                                             }));
    scene_ref->add_child(demo::action_button("libx2", guayaba::Vec2{lx + 10, oy + 14}, "lib x2",
                                             [ops] {
                                                 if (ops.is_valid()) ops->release_twice();
                                             }));
    // No hay boton de "detach": este motor no puede soltar ni cambiar el guion de
    // un nodo vivo, a proposito. El handle de ScriptRef estaria colgando en
    // cuanto algo lo hiciera, y por eso la clase exige que no exista esa
    // operacion. La ficha se queda con su guion mientras el nodo viva.
    scene_ref->add_child(demo::action_button("attach", guayaba::Vec2{right_x + 8, oy + 15},
                                             "attach", [ops] {
                                                 if (ops.is_valid()) ops->attach_pulse();
                                             }));

    return scene;
}

} // namespace demo

#endif // GUAYABA_EXAMPLES_SCENES_CYCLES_LAB_HPP