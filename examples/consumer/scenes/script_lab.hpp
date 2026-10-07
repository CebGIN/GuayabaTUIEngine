#ifndef GUAYABA_EXAMPLES_SCENES_SCRIPT_LAB_HPP
#define GUAYABA_EXAMPLES_SCENES_SCRIPT_LAB_HPP

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include <guayaba/Guayaba.hpp>

#include "nav.hpp"

/*
    Script Lab: los campos de un script se tocan a traves del nodo que lo lleva.

    El patron de la escena, en una linea:

        auto h = nodo->attach_script<LabController>();  // construye, liga y devuelve
        h->speed = 0.2f;                                 // la mitad script, ya tipada
        h.get()->set_local_position({0, 0});            // la mitad nodo, otro handle
        h.is_valid();                                       // false si se fue el nodo o el script

    Los botones de campos hacen exactamente eso sobre el MISMO handle del nodo, y
    preguntan por el guion dentro del clic: si el nodo esta pero ya no lleva
    LabController, el clic no hace nada y no lanza.

    Que se ve, de arriba abajo:

      1 CONTROLADOR  Los tres campos tipados (float, int, bool) y, debajo, el nodo
                      al que pertenecen. Los lee cada frame un nodo DISTINTO
                      (readout), que solo mira y solo escribe texto.
      2 HOOKS        El boton "attach" escribe en el log de la derecha los hooks
                      que se han ido viendo: on_attached, on_enter_tree,
                      on_process, on_exit_tree y on_detached.
      3 RUTAS        get_node("Panel/Boton"), que acierta, y
                      get_node("Panel/Fantasma"), que falla y no lanza nada.

    Abajo, la barra y el bloque amarillo: se mueven solos, asi que la escena esta
    viva aunque no se pulse nada. Solo el raton manda aqui: el backend no lee
    teclado.

    Los textos son ASCII y se recortan al ancho de su columna, porque el motor no
    envuelve lineas: un recorte con "..." informa mas que desbordarse encima del
    panel de al lado.
*/
namespace demo {

namespace script_lab_detail {

// -- texto ------------------------------------------------------------------

// Recorta a width, marcando el recorte. La columna manda: el motor no envuelve.
inline std::string fit(const std::string& text, int width) {
    if (width < 1) return std::string();
    if (static_cast<int>(text.size()) <= width) return text;
    if (width <= 3) return text.substr(0, static_cast<size_t>(width));
    return text.substr(0, static_cast<size_t>(width - 3)) + "...";
}

inline std::string num(float value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.2f", static_cast<double>(value));
    return std::string(buffer);
}

inline std::string num(int value) { return std::to_string(value); }

inline std::string onoff(bool value) { return value ? "1" : "0"; }

// Cuadricula entre dos limites, para los botones que editan los campos: el
// guion va en el final, asi que el mismo helper sirve para speed y para steps.
inline float tune(float value, float low, float high, float step) {
    float next = value + step;
    if (next < low) next = low;
    if (next > high) next = high;
    return next;
}

inline int tune(int value, int low, int high, int step) {
    int next = value + step;
    if (next < low) next = low;
    if (next > high) next = high;
    return next;
}

// La mitad nodo del handle de script, en una llamada.
inline void nudge_home(guayaba::Node2D* node) {
    // El motor anterior necesitaba narrow_to<>() para esto porque su NodeRef no
    // llevaba el tipo del nodo. Este si: NodeRef<Node2D>::get() ya devuelve
    // Node2D*, y un Node2D* nulo se comprueba sin lanzar.
    if (node) node->set_local_position({0, 0});
}

// -- el log de hooks, que es a la vez script y nodo --------------------------

/*
    Vive en un NodeUI y se edita a si mismo. Otro nodo escribe en el log con un
    NodeRef al nodo que lo tiene, no con un puntero, y lo busca con
    get_script_as<S>(), que va vacio en vez de lanzar porque quien escribe es un
    hook.
*/
class HookLog : public guayaba::Script<guayaba::NodeUI> {
public:
    int visible = 12;   // cuantas lineas caben en la columna
    int width = 40;     // ancho de la columna, para el recorte
    std::vector<std::string> lines;

    void push(const std::string& line) {
        lines.push_back(fit(line, width));
        if (visible > 0) {
            while (static_cast<int>(lines.size()) > visible) lines.erase(lines.begin());
        }
        redraw();
    }

    void redraw() {
        guayaba::NodeUI* out = get_owner_as<guayaba::NodeUI>();
        if (!out) return;

        std::vector<std::string> shown;
        if (lines.empty()) {
            shown.push_back("(sin hooks)");
        } else {
            const size_t total = lines.size();
            const size_t room = visible > 0 ? static_cast<size_t>(visible) : 0;
            const size_t skip = total > room ? total - room : 0;
            for (size_t i = skip; i < total; ++i) shown.push_back(lines[i]);
        }
        out->set_text(shown);
    }

    void on_enter_tree() override { push("HookLog enter"); }
    void on_exit_tree() override { push("HookLog exit"); }
    void on_detached() override { push("HookLog detach"); }
};

// -- el script que los botones editan ---------------------------------------

/*
    Tres campos tipados, editables con el raton, y nada mas: ni mapa de strings
    ni casts. El bloque amarillo es suyo, lo mueve desde on_process a traves de
    un NodeRef, asi que cuando el script se suelta el bloque se para: se ve que
    el estado era de verdad suyo.
*/
class LabController : public guayaba::Script<guayaba::Node2D> {
public:
    LabController(guayaba::NodeRef<guayaba::Node> log, guayaba::NodeRef<guayaba::Node2D> box,
                  int min_x, int max_x, int y)
        : log_node(log), mover(box), lane_min(min_x), lane_max(max_x), lane_y(y) {}

    // -- campos tipados: float, int y bool --
    float speed = 0.2f;   // celdas por frame
    int steps = 3;
    bool spin = true;     // rebotar en los bordes en vez de dar la vuelta

    // -- estado de marcha --
    int ticks = 0;
    float travel = 0.0f;
    float direction = 1.0f;

    // Handles, no punteros: este script no posee nada.
    guayaba::NodeRef<guayaba::Node> log_node;
    guayaba::NodeRef<guayaba::Node2D> mover;
    int lane_min = 1;
    int lane_max = 20;
    int lane_y = 15;

    void on_attached() override {
        ticks = 0;
        travel = 0.0f;
        direction = 1.0f;
        note("attach");
    }
    void on_detached() override { note("detach"); }
    void on_enter_tree() override { note("enter"); }
    void on_exit_tree() override { note("exit"); }

    void on_process(double) override {
        ++ticks;
        if (ticks % 30 == 0) note("proc " + num(ticks));

        // try_get_node, no get_node: un hook no lanza excepciones.
        guayaba::Node2D* box = mover.get();
        if (!box) return;
        const float span = static_cast<float>(lane_max - lane_min);
        if (span <= 0.0f) return;

        if (!spin) direction = 1.0f;
        travel += speed * direction;
        if (spin) {
            while (travel > span) { travel = 2.0f * span - travel; direction = -direction; }
            while (travel < 0.0f) { travel = -travel; direction = -direction; }
        } else if (travel > span) {
            travel = 0.0f;
        }
        // Coordenadas de la escena, no de la pantalla: el desplazamiento de los
        // ancestros (main.cpp cuelga el contenido 4 filas mas abajo) lo suma el
        // motor al pintar, asi que aqui no hay que compensarlo a mano.
        box->set_local_position({lane_min + static_cast<int>(travel), lane_y});
    }

private:
    // El log se busca por el nodo que lo tiene. Si ese nodo no estuviera, el hook
    // seguiria funcionando: solo se pierde la linea.
    void note(const std::string& what) {
        if (HookLog* log = script_ptr<HookLog>(log_node)) log->push("Ctrl " + what);
    }
};

// -- el segundo tipo de script, para el boton de swap -----------------------

/*
    Mismo nodo, misma clase base, otro comportamiento: el bloque se mueve con un
    seno. Al pegarlo, el handle del script anterior queda obsoleto (valid() == false)
    y el readout cambia de lectura.
*/
class PulseScript : public guayaba::Script<guayaba::Node2D> {
public:
    PulseScript(guayaba::NodeRef<guayaba::Node> log, guayaba::NodeRef<guayaba::Node2D> box,
                int min_x, int max_x, int y)
        : log_node(log), mover(box), lane_min(min_x), lane_max(max_x), lane_y(y) {}

    float amplitude = 0.25f;   // fraccion del carril
    float rate = 0.45f;       // ritmo del seno
    int ticks = 0;

    guayaba::NodeRef<guayaba::Node> log_node;
    guayaba::NodeRef<guayaba::Node2D> mover;
    int lane_min = 1;
    int lane_max = 20;
    int lane_y = 15;

    void on_attached() override { ticks = 0; note("attach"); }
    void on_detached() override { note("detach"); }
    void on_enter_tree() override { note("enter"); }
    void on_exit_tree() override { note("exit"); }

    void on_process(double delta_time) override {
        ++ticks;
        if (ticks % 30 == 0) note("proc " + num(ticks));

        guayaba::Node2D* box = mover.get();
        if (!box) return;
        const int span = lane_max - lane_min;
        if (span <= 0) return;

        phase += static_cast<double>(rate) * delta_time * 6.0;
        const double offset = std::sin(phase) * static_cast<double>(amplitude)
                            * static_cast<double>(span);
        box->set_local_position({lane_min + span / 2 + static_cast<int>(std::lround(offset)),
                                 lane_y});
    }

private:
    double phase = 0.0;

    void note(const std::string& what) {
        if (HookLog* log = script_ptr<HookLog>(log_node)) log->push("Pulse " + what);
    }
};

// -- el nodo que LEE los campos cada frame -----------------------------------

/*
    Nodo aparte del que lleva el script: no edita nada, solo mira. Por eso mismo
    es el sitio donde se ve caducar un handle sin que nada lance:

      - mientras hay LabController, get_script_as<>() devuelve el guion y salen sus
        tres campos;
      - si el nodo lleva otro guion, no hay ninguno y sale "sin script: ninguno",
        que es la misma linea que se veria tras perderlo.

    LO QUE NO ESTA, Y POR QUE
    Los botones de "detach" y "swap" no se han portado. Este motor no puede soltar
    ni cambiar el guion de un nodo vivo, y el motivo esta escrito en
    Core/ScriptRef.hpp: si un nodo pudiera perder su guion mientras vive, todo
    ScriptRef ya emitido se quedaria colgando y la clase dejaria de poder
    garantizar que script() es el guion de su nodo. Esta escena era la que
    enseñaba a hacer justo eso.

    Tambien escribe la barra de progreso, para que la escena siga viva con
    cualquier script encima (o sin ninguno).
*/
class Readout : public guayaba::Script<guayaba::NodeUI> {
public:
    Readout(guayaba::NodeRef<guayaba::Node> who, guayaba::NodeRef<guayaba::NodeUI> bar_node,
            int bar_cells)
        : subject(who), bar(bar_node), cells(bar_cells) {}

    guayaba::NodeRef<guayaba::Node> subject;
    guayaba::NodeRef<guayaba::NodeUI> bar;
    int cells = 12;
    int frames = 0;

    void on_process(double) override {
        ++frames;

        guayaba::NodeUI* view = get_owner_as<guayaba::NodeUI>();
        if (!view) return;

        // Los dos scripts posibles del sujeto, pedidos en silencio: uno de los
        // dos no existe casi siempre, y este hook no puede lanzar.
        // El handle es del nodo, y el guion se busca en el. Los dos casos se
        // prueban en silencio porque uno de los dos casi nunca existe.
        const LabController* c = script_ptr<LabController>(subject);
        const PulseScript* p = script_ptr<PulseScript>(subject);

        std::string values;
        std::string flags;
        if (c) {
            values = "ctrl speed=" + num(c->speed) + " steps=" + num(c->steps);
            flags = "h.valid=" + onoff(subject.is_valid()) + " spin=" + onoff(c->spin);
        } else if (p) {
            values = "sin amp=" + num(p->amplitude) + " rate=" + num(p->rate);
            flags = "h.valid=" + onoff(subject.is_valid()) + " ph=" + num(p->ticks % 100);
        } else {
            values = "sin script: ninguno";
            flags = "h.valid=0";
        }
        flags += " t=" + num(frames % 1000);

        // La mitad nodo del handle: node() devuelve OTRO NodeRef al mismo nodo,
        // valido mientras el nodo exista, y no necesita ningun cast.
        // El handle del nodo es el mismo en los dos casos: lo que cambia es que
        // guion lleva, y eso ya se ha comprobado arriba.
        guayaba::NodeRef<guayaba::Node> half = subject;
        const std::string owner = half.is_valid() ? "h.get()=" + half->get_name() : "h.get()=?";

        view->set_text({values, flags, owner});

        if (guayaba::NodeUI* gauge = bar.get()) gauge->set_text({bar_line()});
    }

private:
    std::string bar_line() const {
        const int width = cells > 1 ? cells : 1;
        const int filled = (frames / 2) % (width + 1);
        const int percent = filled * 100 / width;

        std::string line = "viva [";
        line.append(static_cast<size_t>(filled), '#');
        line.append(static_cast<size_t>(width - filled), '.');
        line += "] ";
        char tail[8];
        std::snprintf(tail, sizeof(tail), "%3d%%", percent);
        line += tail;
        return line;
    }
};

// -- el nodo que resuelve rutas ----------------------------------------------

/*
    get_node no lanza: un acierto es un handle y un fallo es un handle vacio. Las
    dos lineas se comprueban cada frame, asi que el acierto y el fallo se ven a
    la vez y en el mismo sitio.
*/
class PathProbe : public guayaba::Script<guayaba::NodeUI> {
public:
    PathProbe(guayaba::NodeRef<guayaba::Node> root, guayaba::NodeRef<guayaba::NodeUI> out)
        : tree(root), view(out) {}

    guayaba::NodeRef<guayaba::Node> tree;
    guayaba::NodeRef<guayaba::NodeUI> view;
    int probes = 0;

    void on_process(double) override {
        ++probes;

        guayaba::NodeUI* out = view.get();
        if (!out || !tree) return;

        guayaba::NodeRef<guayaba::Node> hit = tree->get_node_or_null<guayaba::Node>("Panel/Boton");
        guayaba::NodeRef<guayaba::Node> miss = tree->get_node_or_null<guayaba::Node>("Panel/Fantasma");

        const std::string found = hit ? "Panel/Boton -> " + fit(hit->get_name(), 6)
                                      : "Panel/Boton -> no encontrado";
        out->set_text({found,
                       "Panel/Fantasma ->",
                       miss ? "  OK (no deberia)" : "  no encontrado"});
    }
};

} // namespace script_lab_detail

/*
    La escena entera. Se construye con unique_ptr + add_child (quien construye es
    dueno de lo que crea) y en cuanto existe el puntero crudo se convierte en
    handle con make_node_ref. A partir de ahi, en las lambdas y en los scripts,
    solo hay NodeRef.

    La construccion mide la pantalla con demo::screen_size() y reparte dos
    columnas: a la izquierda el controlador y las rutas, a la derecha el log de
    hooks. Con 40x20 (el minimo) caben las tres secciones; con una pantalla ancha
    las columnas se separan y el log aprovecha el ancho.
*/
inline std::unique_ptr<guayaba::Node> build_script_lab() {
    const guayaba::Vec2 screen = demo::screen_size({100, 60});
    const int rows = std::max(screen.y - 4, 10);   // main.cpp baja el contenido 4 filas
    const int left_w = std::min(std::max(screen.x / 2 - 2, 24), 58);
    const int right_x = left_w + 2;
    const int right_w = std::max(screen.x - right_x - 1, 10);
    // Si sobra alto, el bloque baja un poco, no se va al medio de la pantalla.
    const int oy = rows > 16 ? std::min((rows - 16) / 2, 4) : 0;
    const int lx = 1;

    const int lane_min = lx;
    const int lane_max = lx + left_w - 4;
    const int lane_y = oy + 15;
    const int bar_cells = left_w - 12;

    auto scene = std::make_unique<guayaba::Node2D>("script_lab", guayaba::Vec2{0, 0});
    guayaba::NodeRef<guayaba::Node2D> scene_ref = make_node_ref<guayaba::Node2D>(scene.get());

    // Panel: no dibuja nada. Existe para que "Panel/Boton" sea una ruta de verdad
    // y para que el boton reset quede debajo de un grupo, no suelto.
    auto panel_node = std::make_unique<guayaba::Node2D>("Panel", guayaba::Vec2{0, 0});
    guayaba::NodeRef<guayaba::Node2D> panel_ref = make_node_ref<guayaba::Node2D>(panel_node.get());
    scene_ref->add_child(std::move(panel_node));

    // El sujeto: el nodo cuyo script se demuestra. No dibuja nada, solo lleva la
    // logica. Su handle es NodeRef<Node2D> porque el nodo es un Node2D, y asi el
    // resto de la escena lo usa con su tipo (lane_min/lane_max) sin castear.
    auto subject_node = std::make_unique<guayaba::Node2D>("Controlador", guayaba::Vec2{0, 0});
    guayaba::NodeRef<guayaba::Node2D> subject_ref =
        make_node_ref<guayaba::Node2D>(subject_node.get());
    panel_ref->add_child(std::move(subject_node));

    // El bloque amarillo, que los scripts mueven.
    auto mover_node = std::make_unique<guayaba::NodeBox>(
        "mover", guayaba::Vec2{lane_min, lane_y}, guayaba::Vec2{3, 1},
        guayaba::Color::BRIGHT_YELLOW);
    guayaba::NodeRef<guayaba::Node2D> mover_ref = make_node_ref<guayaba::Node2D>(mover_node.get());
    scene_ref->add_child(std::move(mover_node));

    // El log, primero: los demas scripts escriben en el y asi sus hooks aparecen
    // en orden desde el principio.
    auto log_node = demo::text_block("log", guayaba::Vec2{right_x, oy + 1}, {"(sin hooks)"},
                                     guayaba::Color::BRIGHT_CYAN);
    guayaba::NodeRef<guayaba::NodeUI> log_ref = make_node_ref<guayaba::NodeUI>(log_node.get());
    scene_ref->add_child(std::move(log_node));

    auto log_handle = log_ref->attach_script<script_lab_detail::HookLog>();
    log_handle->visible = std::min(12, rows - oy - 3);
    log_handle->width = right_w;
    log_handle->push("panel listo");

    // El script del sujeto, escrito con el patron completo: la clase se nombra
    // UNA vez y despues solo hay handle.
    auto controller = subject_ref->attach_script<script_lab_detail::LabController>(
        log_ref, mover_ref, lane_min, lane_max, lane_y);
    controller->speed = 0.2f;
    controller->steps = 3;
    controller->spin = true;
    script_lab_detail::nudge_home(controller.node());
    (void)controller.is_valid();

    // Cabeceras de seccion.
    scene_ref->add_child(demo::text_line("head1", guayaba::Vec2{lx, oy},
                                         "1 CONTROLADOR (NodeRef)", guayaba::Color::BRIGHT_CYAN));
    scene_ref->add_child(demo::text_line("head2", guayaba::Vec2{lx, oy + 7},
                                         "2 HOOKS DEL SCRIPT", guayaba::Color::BRIGHT_CYAN));
    scene_ref->add_child(demo::text_line("head3", guayaba::Vec2{lx, oy + 10}, "3 RUTAS",
                                         guayaba::Color::BRIGHT_CYAN));
    scene_ref->add_child(demo::text_line("log_head", guayaba::Vec2{right_x, oy},
                                         "LOG DE HOOKS", guayaba::Color::BRIGHT_YELLOW));

    // La barra: la escribe el readout, asi que nace antes que el.
    auto bar_node = demo::text_line("barra", guayaba::Vec2{lx, oy + 14}, "viva []",
                                   guayaba::Color::BRIGHT_GREEN);
    guayaba::NodeRef<guayaba::NodeUI> bar_ref = make_node_ref<guayaba::NodeUI>(bar_node.get());
    scene_ref->add_child(std::move(bar_node));

    // El readout: un nodo que solo lee. Su on_process repinta estas tres lineas.
    auto readout_node = demo::text_block("readout", guayaba::Vec2{lx, oy + 1},
                                         {"ctrl speed=0.20 steps=3", "h.valid=1 spin=1 t=0",
                                          "h.get()=Controlador"},
                                         guayaba::Color::BRIGHT_WHITE);
    guayaba::NodeRef<guayaba::NodeUI> readout_ref =
        make_node_ref<guayaba::NodeUI>(readout_node.get());
    scene_ref->add_child(std::move(readout_node));
    auto readout = readout_ref->attach_script<script_lab_detail::Readout>(subject_ref, bar_ref,
                                                                        bar_cells);
    // El handle que devuelve attach_script ya es valido: solo hay que mirarlo.
    (void)readout.is_valid();

    // Las rutas: un acierto y un fallo deliberado.
    auto path_node = demo::text_block("rutas", guayaba::Vec2{lx, oy + 11},
                                      {"Panel/Boton -> Boton", "Panel/Fantasma ->",
                                       "  no encontrado"},
                                      guayaba::Color::BRIGHT_YELLOW);
    guayaba::NodeRef<guayaba::NodeUI> path_ref = make_node_ref<guayaba::NodeUI>(path_node.get());
    scene_ref->add_child(std::move(path_node));
    auto probe = path_ref->attach_script<script_lab_detail::PathProbe>(scene_ref, path_ref);
    (void)probe.is_valid();

    // -- botones ------------------------------------------------------------
    // Cada uno captura handles y nada mas. Los de campos capturan el handle
    // TIPADO, no el nodo: si el script se suelta, valid() lo dice y el clic se
    // queda sin hacer nada en vez de lanzar.

    const guayaba::NodeRef<guayaba::Node2D> fields = make_node_ref<guayaba::Node2D>(controller.node());

    panel_ref->add_child(demo::action_button(
        "vel_down", guayaba::Vec2{lx, oy + 4}, "-0.05", [fields] {
            if (script_lab_detail::LabController* f = script_ptr<script_lab_detail::LabController>(fields)) {
                f->speed = script_lab_detail::tune(f->speed, 0.0f, 0.9f, -0.05f);
            }
        }));

    panel_ref->add_child(demo::action_button(
        "vel_up", guayaba::Vec2{lx + 8, oy + 4}, "+0.05", [fields] {
            if (script_lab_detail::LabController* f = script_ptr<script_lab_detail::LabController>(fields)) {
                f->speed = script_lab_detail::tune(f->speed, 0.0f, 0.9f, 0.05f);
            }
        }));

    panel_ref->add_child(demo::action_button(
        "step_down", guayaba::Vec2{lx, oy + 5}, "-1", [fields] {
            if (script_lab_detail::LabController* f = script_ptr<script_lab_detail::LabController>(fields)) {
                f->steps = script_lab_detail::tune(f->steps, 0, 9, -1);
            }
        }));

    panel_ref->add_child(demo::action_button(
        "step_up", guayaba::Vec2{lx + 5, oy + 5}, "+1", [fields] {
            if (script_lab_detail::LabController* f = script_ptr<script_lab_detail::LabController>(fields)) {
                f->steps = script_lab_detail::tune(f->steps, 0, 9, 1);
            }
        }));

    panel_ref->add_child(demo::action_button(
        "spin", guayaba::Vec2{lx, oy + 6}, "spin", [fields] {
            if (script_lab_detail::LabController* f = script_ptr<script_lab_detail::LabController>(fields)) {
                f->spin = !f->spin;
            }
        }));

    // El que se busca con la ruta "Panel/Boton": ademas de reset, es el acierto
    // de la seccion de rutas.
    panel_ref->add_child(demo::action_button(
        "Boton", guayaba::Vec2{lx + 8, oy + 6}, "reset", [fields] {
            if (script_lab_detail::LabController* f = script_ptr<script_lab_detail::LabController>(fields)) {
                f->speed = 0.2f;
                f->steps = 3;
                f->spin = true;
            }
        }));

    // -- ciclo de vida ------------------------------------------------------

    // attach construye, liga y devuelve el handle, con el tipo nombrado una vez.
    // Un script pegado a un nodo que ya esta en el arbol no recibe on_enter_tree
    // solo, asi que se pide a mano, como documenta Script.hpp.
    panel_ref->add_child(demo::action_button(
        "attach", guayaba::Vec2{lx, oy + 8}, "attach",
        [subject_ref, log_ref, mover_ref, lane_min, lane_max, lane_y] {
            auto h = subject_ref->attach_script<script_lab_detail::LabController>(
                log_ref, mover_ref, lane_min, lane_max, lane_y);
            h->speed = 0.2f;
            h->steps = 3;
            h->spin = true;
            script_lab_detail::nudge_home(h.node());
            (void)h.is_valid();
            if (h.node() && h.node()->is_in_tree()) h->on_enter_tree();
        }));

    // No hay botones de "detach" ni de "swap", y no es una omision: este motor no
    // puede soltar ni cambiar el guion de un nodo vivo. attach_script() sella el
    // hueco en el primer attach y no hay desenlace. La razon esta escrita en
    // Core/ScriptRef.hpp: si un nodo pudiera perder su guion mientras vive, todo
    // ScriptRef ya emitido se quedaria colgando, y la clase dejaria de poder
    // assurance de que script() es el guion de su nodo. Ese par de botones
    // enseñaba justo lo que este motor decide no permitir.
    //
    // Lo que si se conserva es la mitadUa vida: el log engancha los hooks
    // (on_attached, on_enter_tree, on_process, on_exit_tree) y el readout sigue
    // al guion mientras el nodo viva, que es el resto de la escena.

    return scene;
}

} // namespace demo

#endif // GUAYABA_EXAMPLES_SCENES_SCRIPT_LAB_HPP