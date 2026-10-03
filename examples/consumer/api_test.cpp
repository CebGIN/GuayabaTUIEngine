/*
    Criterio de aceptacion 3: incluir el motor no debe filtrar macros del sistema
    operativo. Si <windows.h> se colara por alguna via, esto rompe la compilacion.
*/

#include <guayaba/Guayaba.hpp>

#ifdef min
    #error "Fuga de macro: 'min' esta definida tras incluir Guayaba.hpp"
#endif
#ifdef max
    #error "Fuga de macro: 'max' esta definida tras incluir Guayaba.hpp"
#endif
#ifdef CreateFile
    #error "Fuga de macro: se ha incluido <windows.h> en la API publica"
#endif
#ifdef termios
    #error "Fuga de tipo: se ha incluido <termios.h> en la API publica"
#endif

using namespace guayaba;

#include <cassert>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

static int failures = 0;

static void check(bool condition, const std::string& what) {
    if (condition) {
        std::cout << "  ok   " << what << "\n";
    } else {
        std::cout << "  FAIL " << what << "\n";
        ++failures;
    }
}

/*
    Backend falso: demuestra que IOSTerminal es implementable desde fuera de la
    libreria con la sola API publica, y permite ejercitar el motor sin terminal.
*/
class MockTerminal : public IOSTerminal {
public:
    bool initialized = false;
    bool restored = false;
    bool cursor_visible = false;
    Vec2 cursor = {0, 0};
    Vec2 screen = {40, 20};
    std::vector<TermChar> last_frame;
    Vec2 last_frame_size{0, 0};

    Vec2 fake_mouse = {0, 0};
    bool fake_l = false;
    bool fake_r = false;
    bool fake_l_just = false;
    bool fake_r_just = false;

    void init() override { initialized = true; }
    void restore() override { restored = true; }
    void set_cursor_visible(bool visible) override { cursor_visible = visible; }
    void set_cursor_position(Vec2 pos) override { cursor = pos; }

    void present_buffer(const std::vector<TermChar>& buffer, Vec2 buffer_size) override {
        last_frame = buffer;
        last_frame_size = buffer_size;
    }

    Vec2 get_screen_size(Vec2) const override { return screen; }

    void poll_events() override {}
    Vec2 get_mouse_position() const override { return fake_mouse; }
    bool is_left_click_pressed() const override { return fake_l; }
    bool is_right_click_pressed() const override { return fake_r; }
    bool is_left_click_just_pressed() const override { return fake_l_just; }
    bool is_right_click_just_pressed() const override { return fake_r_just; }
    std::string get_line_input(Vec2) override { return ""; }
};

/*
    Node expone enter_tree/exit_tree como protected y solo SceneManager puede
    dispararlas. Este subclase las expone para poder ejercitar los eventos de
    arbol en un test sin levantar una escena de verdad.
*/
class TreeProbe : public Node {
public:
    explicit TreeProbe(const std::string& node_name = "probe") : Node(node_name) {}
    void enter() { enter_tree(); }
    void leave() { exit_tree(); }
};

/*
    Registra cada hook que recibe, con un prefijo para poder distinguir scripts
    distintos al comprobar el orden global.
*/
class TraceScript : public Script {
public:
    std::string label = "s";
    std::vector<std::string> trace;
    std::shared_ptr<std::vector<std::string>> shared_trace;

    void record(const std::string& what) {
        trace.push_back(label + ":" + what);
        if (shared_trace) shared_trace->push_back(label + ":" + what);
    }

    void on_attached() override { record("attached"); }
    void on_detached() override { record("detached"); }
    void on_enter_tree() override { record("enter_tree"); }
    void on_exit_tree() override { record("exit_tree"); }
    void on_process(double) override { record("process"); }
    void on_draw(ConsoleRenderer&) override { record("draw"); }
};

// Tipo distinto, para comprobar que get_script_as<> rechaza un tipo ajeno.
class OtherScript : public Script {
public:
    int unused = 0;
};

// El patron documentado: un script adjunto a un nodo vivo pide on_enter_tree
// a mano, porque no le llegara solo.
class OptInScript : public Script {
public:
    bool got_enter_tree = false;
    void on_attached() override {
        if (get_owner() && get_owner()->is_in_tree()) on_enter_tree();
    }
    void on_enter_tree() override { got_enter_tree = true; }
};

// El caso real: logica y estado propios como campos tipados, sin mapa ni casts.
class Mover : public Script {
public:
    int offset = 0;
    float speed = 1.5f;
    void on_process(double delta_time) override {
        offset += static_cast<int>(speed * delta_time);
    }
};

static void test_vec2() {
    std::cout << "Vec2\n";
    check((Vec2{1, 2} + Vec2{3, 4}) == (Vec2{4, 6}), "operator+");
    check((Vec2{3, 4} - Vec2{1, 2}) == (Vec2{2, 2}), "operator-");
    check((Vec2{1, 2} == Vec2{1, 2}), "operator==");
    check((Vec2{1, 2} != Vec2{1, 3}), "operator!=");
    Vec2 zero;
    check(zero.x == 0 && zero.y == 0, "valores por defecto");
}

static void test_color() {
    std::cout << "Color\n";
    check(Color::from_name(Color::BLACK) == 0, "BLACK == 0");
    check(Color::from_name(Color::RED) == 1, "RED == 1");
    check(Color::from_name(Color::WHITE) == 7, "WHITE == 7");
    check(Color::from_name(Color::BRIGHT_RED) == 9, "BRIGHT_RED == 9");
    check(Color::from_name(Color::BRIGHT_WHITE) == 15, "BRIGHT_WHITE == 15");
    check(Color::from_name("BRIGHT_BLACK") == 8, "BRIGHT_BLACK == 8");
    check(Color::from_name("no existe") == 7, "desconocido cae en blanco");
    // Las constantes son constexpr const char*: deben bindear a const std::string&.
    const std::string& ref = Color::GREEN;
    check(ref == "GREEN", "const char* bindea a const std::string&");
}

static void test_renderer(MockTerminal& mock) {
    std::cout << "ConsoleRenderer\n";
    ConsoleRenderer renderer({10, 4}, &mock);
    check(renderer.get_buffer_size() == (Vec2{10, 4}), "buffer inicial");

    renderer.put_string({0, 0}, "Hola", 1, 7);
    renderer.present();
    check(mock.last_frame_size == (Vec2{10, 4}), "present() pasa el tamano");
    check(mock.last_frame.size() == 40, "present() pasa los 40 TermChar");
    check(mock.last_frame[0].character == L'H', "primer caracter");
    check(mock.last_frame[1].character == L'o', "segundo caracter");
    check(mock.last_frame[5].character == L' ', "clear_buffer() deja espacios");

    // Fuera de rango: no debe escribir ni reventar.
    renderer.put_char({100, 100}, L'X', 1, 7);
    renderer.put_char({-1, 0}, L'X', 1, 7);
    renderer.present();
    check(mock.last_frame[0].character == L'H', "put_char ignora fuera de rango");

    renderer.resize({20, 6});
    check(renderer.get_buffer_size() == (Vec2{20, 6}), "resize");
    renderer.resize({0, 0});
    check(renderer.get_buffer_size() == (Vec2{20, 6}), "resize rechaza 0x0");

    // Un renderer sin terminal no debe fallar al presentar.
    ConsoleRenderer orphan({4, 2}, nullptr);
    orphan.put_char({0, 0}, L'Z', 0, 0);
    orphan.present();
    check(true, "present() sin terminal es no-op");
}

static void test_node_tree() {
    std::cout << "Node\n";
    auto root = std::make_shared<Node>("root");
    auto child = std::make_shared<Node>("child");
    auto grand = std::make_shared<Node>("grand");

    check(root->is_root(), "root sin padre es root");
    root->add_child(child);
    check(!child->is_root(), "child con padre no es root");
    check(root->get_child_count() == 1, "get_child_count");
    check(root->get_child(0)->get_name() == "child", "get_child por indice");
    check(child->get_parent()->get_name() == "root", "get_parent");
    child->add_child(grand);
    check(grand->get_parent_of_type<Node>() == child, "get_parent_of_type<Node>");

    bool threw = false;
    try { root->get_child(99); } catch (const std::out_of_range&) { threw = true; }
    check(threw, "get_child fuera de rango lanza out_of_range");

    int walked = 0;
    for (const auto& n : *root) { (void)n; ++walked; }
    check(walked == 1, "begin()/end() itera los hijos");

    check(root->get_type() == NodeType::NODE, "Node::get_type");
}

static void test_node2d_and_cache() {
    std::cout << "Node2D\n";
    auto parent = std::make_shared<Node2D>("parent", Vec2{10, 5});
    auto child = std::make_shared<Node2D>("child", Vec2{3, 2});
    parent->add_child(child);

    check(child->get_local_position() == (Vec2{3, 2}), "get_local_position");
    check(child->get_global_position() == (Vec2{13, 7}), "get_global_position suma el padre");
    check(child->get_type() == NodeType::NODE2D, "Node2D::get_type");

    parent->set_local_position({20, 5});
    check(child->get_global_position() == (Vec2{23, 7}), "la cache se invalida al mover el padre");

    child->set_global_position({0, 0});
    check(child->get_local_position() == (Vec2{-20, -5}), "set_global_position");
    check(child->get_global_position() == (Vec2{0, 0}), "set_global_position fija la cache");

    // get_parent_of_type<Node2D> debe encontrar el padre aunque sea indirecto.
    auto mid = std::make_shared<Node>("mid");
    auto leaf = std::make_shared<Node2D>("leaf", Vec2{1, 1});
    mid->add_child(leaf);
    parent->add_child(mid);
    check(leaf->get_parent_of_type<Node2D>() == parent, "get_parent_of_type<Node2D> cruza un Node intermedio");
}

static void test_ui_nodes(MockTerminal& mock) {
    std::cout << "NodeUI / NodeBox / NodeSQ / NodeButton\n";
    auto label = std::make_shared<NodeUI>("label", Vec2{2, 3}, std::vector<std::string>{"abcd", "ef"});
    check(label->get_size() == (Vec2{4, 2}), "NodeUI recalcula el tamano");
    check(label->get_type() == NodeType::NODEUI, "NodeUI::get_type");
    check(label->is_inside({2, 3}), "is_inside borde superior izquierdo");
    check(label->is_inside({5, 4}), "is_inside borde inferior derecho");
    check(!label->is_inside({6, 3}), "is_inside fuera por x");
    label->set_text({"xy"});
    check(label->get_size() == (Vec2{2, 1}), "set_text recalcula el tamano");
    label->change_text_color(Color::RED);
    label->change_background_color(Color::BLUE);

    auto box = std::make_shared<NodeBox>("box", Vec2{0, 0}, Vec2{3, 2}, Color::BLACK);
    check(box->get_size() == (Vec2{3, 2}), "NodeBox::get_size");
    box->set_size({5, 5});
    check(box->get_size() == (Vec2{5, 5}), "NodeBox::set_size");
    box->change_background_color(Color::WHITE);

    auto sq = std::make_shared<NodeSQ>("sq", Vec2{0, 0}, Vec2{5, 3}, Color::WHITE, Color::BLACK);
    check(sq->get_size() == (Vec2{5, 3}), "NodeSQ::get_size");
    sq->change_text_color(Color::CYAN);
    sq->change_background_color(Color::MAGENTA);

    // Dibujo real contra el backend falso.
    auto scene = std::make_shared<Node>("scene");
    scene->add_child(sq);
    scene->add_child(label);
    ConsoleRenderer renderer({20, 8}, &mock);
    renderer.clear_buffer();
    scene->draw(renderer);
    renderer.present();

    const int W = renderer.get_buffer_size().x;
    auto idx = [W](int x, int y){ return y * W + x; };

    check(mock.last_frame[idx(0, 0)].character == L'-', "NodeSQ dibuja el borde superior izq.");
    check(mock.last_frame[idx(4, 0)].character == L'-', "NodeSQ dibuja el borde superior der.");
    check(mock.last_frame[idx(0, 1)].character == L'|', "NodeSQ dibuja el borde izquierdo");
    check(mock.last_frame[idx(4, 1)].character == L'|', "NodeSQ dibuja el borde derecho");
    check(mock.last_frame[idx(0, 2)].character == L'-', "NodeSQ dibuja el borde inferior");
    check(mock.last_frame[idx(2, 3)].character == L'x', "NodeUI dibuja su texto");
    check(mock.last_frame[idx(3, 3)].character == L'y', "NodeUI dibuja su texto (2)");

    // Boton: hover + clic.
    auto button = std::make_shared<NodeButton>("btn", Vec2{10, 5}, Color::BLACK, Color::WHITE,
                                               std::vector<std::string>{"OK"});
    int clicks = 0;
    button->set_on_click([&clicks]{ ++clicks; });
    check(button->get_type() == NodeType::NODEBUTTON, "NodeButton::get_type");

    Input::g_terminal = &mock;
    Input::mouse_pos = {50, 50};
    Input::l_click_just_pressed = false;
    button->update(0.016);
    check(!button->is_hovered(), "sin hover con el raton lejos");
    check(clicks == 0, "sin clic sin hover");

    Input::mouse_pos = {10, 5};
    button->update(0.016);
    check(button->is_hovered(), "hover con el raton encima");

    Input::l_click_just_pressed = true;
    button->update(0.016);
    check(clicks == 1, "el clic dispara on_click");

    Input::l_click_just_pressed = false;
    button->update(0.016);
    check(clicks == 1, "no se dispara dos veces");

    Input::g_terminal = nullptr;
    Input::mouse_pos = {0, 0};
}

static void test_input_delegation() {
    std::cout << "Input\n";
    MockTerminal mock;
    Input::g_terminal = nullptr;
    Input::refresh_input();
    check(Input::g_terminal == nullptr, "refresh_input() sin terminal no revienta");

    mock.fake_mouse = {7, 8};
    mock.fake_l = true;
    mock.fake_l_just = true;
    Input::g_terminal = &mock;
    Input::refresh_input();
    check(Input::mouse_pos == (Vec2{7, 8}), "refresh_input() propaga el raton");
    check(Input::l_click, "refresh_input() propaga el clic izquierdo");
    check(Input::l_click_just_pressed, "refresh_input() propaga just_pressed");
    check(Input::l_click_prev == false, "l_click_prev se actualiza al.Refresh");

    Input::refresh_input();
    check(Input::l_click_prev == true, "l_click_prev refleja el frame anterior");
    check(Input::l_click_just_pressed == true, "el backend decide just_pressed");

    Input::g_terminal = nullptr;
}

static void test_scene_manager() {
    std::cout << "SceneManager\n";
    auto& sm = SceneManager::get_instance();
    check(&sm == &SceneManager::get_instance(), "get_instance() es un singleton");

    auto scene_a = std::make_shared<Node>("scene_a");
    auto scene_b = std::make_shared<Node>("scene_b");

    sm.change_scene(scene_a);
    check(sm.get_current_scene() == nullptr, "change_scene difiere la escena (no aplica al instante)");

    sm.increase_frame_count();
    check(sm.get_frame_count() == 1, "get_frame_count");
    sm.stop_running();
    check(true, "stop_running() no lanza");
}

static void test_pui_prefabs() {
    std::cout << "pui\n";
    auto offset = std::make_shared<int>(50);
    auto viewport = std::make_shared<Node2D>("viewport", Vec2{0, 0});

    auto slider = pui::h_slider(viewport, offset, 20, Color::WHITE, Color::BLACK, Color::BLACK, Color::WHITE);
    check(slider != nullptr, "h_slider devuelve un Node");
    check(slider->get_child_count() == 1, "h_slider monta el boton deslizador");

    auto vslide = pui::v_slider(viewport, offset, 20, Color::WHITE, Color::BLACK, Color::BLACK, Color::WHITE);
    check(vslide != nullptr, "v_slider devuelve un Node");
    check(vslide->get_child_count() == 1, "v_slider monta el boton deslizador");

    bool confirmed = false;
    auto dialog = pui::confirm_dialog("Hola", [&confirmed]{ confirmed = true; });
    check(dialog != nullptr, "confirm_dialog devuelve un Node");
    check(dialog->get_child_count() == 3, "confirm_dialog monta titulo, cancelar y aceptar");
    (void)confirmed;
}

static void test_script(MockTerminal& mock) {
    std::cout << "Script\n";
    ConsoleRenderer renderer({20, 8}, &mock);

    // -- Ciclo del SCRIPT: adjuntar y desligar, sin tocar el arbol.
    auto probe = std::make_shared<TreeProbe>("probe");
    auto trace = std::make_shared<TraceScript>();
    check(!probe->has_script(), "nodo nuevo sin script");
    probe->attach_script(trace);
    check(probe->has_script(), "attach_script engancha el script");
    check(probe->get_script() == trace, "get_script devuelve lo enganchado");
    check(trace->get_owner() == probe, "get_owner devuelve el nodo anfitrion");
    check(trace->trace == std::vector<std::string>{"s:attached"},
          "adjuntar dispara solo on_attached");

    probe->attach_script(nullptr);
    check(probe->get_script() == trace, "adjuntar null es no-op");

    trace->trace.clear();
    probe->attach_script(trace);
    check(trace->trace.empty(), "re-adjuntar el mismo script es no-op");

    auto victim = std::make_shared<Node>("victim");
    auto bound = std::make_shared<TraceScript>();
    victim->attach_script(bound);
    auto other = std::make_shared<Node>("other");
    other->attach_script(bound);
    check(bound->get_owner() == victim, "no se roba un script ya ligado a otro nodo");
    check(!other->has_script(), "el nodo que no pudo robarlo se queda sin script");

    // -- Ciclo del NODO: entrar y salir del arbol.
    check(!probe->is_in_tree(), "is_in_tree falso antes de entrar");
    probe->enter();
    check(probe->is_in_tree(), "is_in_tree true tras entrar");
    check(trace->trace == std::vector<std::string>{"s:enter_tree"},
          "entrar al arbol dispara on_enter_tree");
    probe->leave();
    check(trace->trace == std::vector<std::string>{"s:enter_tree", "s:exit_tree"},
          "salir del arbol dispara on_exit_tree");
    check(!probe->is_in_tree(), "is_in_tree falso tras salir");

    // -- Adjuntar a un nodo ya vivo NO inventa una entrada al arbol.
    auto live = std::make_shared<TreeProbe>("live");
    live->enter();
    auto late = std::make_shared<TraceScript>();
    live->attach_script(late);
    check(late->trace == std::vector<std::string>{"s:attached"},
          "adjuntar a un nodo vivo no dispara on_enter_tree");
    auto optin = std::make_shared<OptInScript>();
    live->attach_script(optin);
    check(optin->got_enter_tree, "on_attached puede pedir on_enter_tree a mano");
    check(late->trace == std::vector<std::string>{"s:attached", "s:detached"},
          "sustituir dispara on_detached en el script anterior");
    live->leave();
    check(late->trace == std::vector<std::string>{"s:attached", "s:detached"},
          "el script ya desligado no recibe on_exit_tree");

    // -- detach
    auto det = std::make_shared<TreeProbe>("det");
    auto det_script = std::make_shared<TraceScript>();
    det->attach_script(det_script);
    det->enter();
    det_script->trace.clear();
    det->detach_script();
    check(det_script->trace == std::vector<std::string>{"s:detached"},
          "detach dispara solo on_detached");
    check(!det->has_script(), "detach deja el nodo sin script");
    check(det_script->get_owner() == nullptr, "detach desliga el owner");
    det->detach_script();
    check(true, "detach sin script es no-op");

    // -- Sustitucion
    auto rep = std::make_shared<TreeProbe>("rep");
    auto old_script = std::make_shared<TraceScript>();
    auto new_script = std::make_shared<TraceScript>();
    rep->attach_script(old_script);
    rep->enter();
    old_script->trace.clear();
    rep->attach_script(new_script);
    check(old_script->trace == std::vector<std::string>{"s:detached"},
          "sustituir dispara on_detached, nunca on_exit_tree");
    check(old_script->get_owner() == nullptr, "el script sustituido queda desligado");
    check(rep->get_script() == new_script, "el nodo queda con el script nuevo");
    check(new_script->trace == std::vector<std::string>{"s:attached"},
          "el script nuevo recibe on_attached");

    // -- update y draw sobre un tipo real (no un Node pelado).
    auto sq = std::make_shared<NodeSQ>("sq", Vec2{0, 0}, Vec2{4, 3}, Color::WHITE, Color::BLACK);
    auto sq_script = std::make_shared<TraceScript>();
    sq->attach_script(sq_script);
    check(sq_script->trace == std::vector<std::string>{"s:attached"},
          "adjuntar dispara on_attached sobre un NodeSQ");
    sq_script->trace.clear();
    sq->update(0.016);
    check(sq_script->trace == std::vector<std::string>{"s:process"},
          "update propaga on_process a un NodeSQ");
    sq_script->trace.clear();
    sq->draw(renderer);
    check(sq_script->trace == std::vector<std::string>{"s:draw"},
          "draw propaga on_draw a un NodeSQ");

    // -- Orden: el on_draw del padre va antes que el del hijo, para que el del
    //    hijo siga siendo un overlay de verdad.
    auto parent = std::make_shared<Node>("parent");
    auto child = std::make_shared<Node>("child");
    parent->add_child(child);
    auto parent_script = std::make_shared<TraceScript>();
    auto child_script = std::make_shared<TraceScript>();
    parent_script->label = "p";
    child_script->label = "c";
    auto order = std::make_shared<std::vector<std::string>>();
    parent_script->shared_trace = order;
    child_script->shared_trace = order;
    parent->attach_script(parent_script);
    child->attach_script(child_script);
    order->clear();
    parent->draw(renderer);
    check(*order == std::vector<std::string>{"p:draw", "c:draw"},
          "on_draw del padre precede al del hijo");

    // -- Acceso tipado a propiedades.
    check(probe->get_script_as<TraceScript>() == trace, "get_script_as con el tipo correcto");
    check(probe->get_script_as<OtherScript>() == nullptr,
          "get_script_as con un tipo ajeno devuelve nullptr");
    check(!probe->has_script() || probe->get_script_as<TraceScript>() != nullptr,
          "get_script_as sobre nodo sin script devuelve nullptr");
    check(sq_script->get_owner_as<NodeSQ>() == sq, "get_owner_as<NodeSQ> da el tipo concreto");
    check(sq_script->get_owner_as<NodeUI>() == nullptr,
          "get_owner_as con un tipo ajeno devuelve nullptr");
    check(sq_script->get_script_name() == "Unnamed Script", "get_script_name por defecto");
    sq_script->set_script_name("slider");
    check(sq_script->get_script_name() == "slider", "set_script_name");

    // -- Campos tipados: sin mapa, sin casts, sin macros.
    auto host = std::make_shared<Node>("host");
    auto mover = std::make_shared<Mover>();
    host->attach_script(mover);
    mover->speed = 2.0f;
    mover->offset = 5;
    host->update(1.0);
    check(mover->offset == 7, "on_process acumula estado propio tipado");
    check(host->get_script_as<Mover>()->offset == 7,
          "el mismo campo se lee a traves del nodo, ya tipado");

    // -- update recorre los hijos, igual que draw.
    auto upd_parent = std::make_shared<Node>("up");
    auto upd_child = std::make_shared<Node>("uc");
    upd_parent->add_child(upd_child);
    auto upd_parent_script = std::make_shared<TraceScript>();
    auto upd_child_script = std::make_shared<TraceScript>();
    upd_parent_script->label = "p";
    upd_child_script->label = "c";
    upd_parent->attach_script(upd_parent_script);
    upd_child->attach_script(upd_child_script);
    upd_parent_script->trace.clear();
    upd_child_script->trace.clear();
    upd_parent->update(0.016);
    check(upd_parent_script->trace == std::vector<std::string>{"p:process"}
          && upd_child_script->trace == std::vector<std::string>{"c:process"},
          "update propaga on_process a los hijos");

    // -- El enlace debil evita el ciclo Node -> Script -> Node.
    std::weak_ptr<Node> weak_node;
    std::shared_ptr<Script> survivor;
    {
        auto doomed = std::make_shared<Node>("doomed");
        auto orphan = std::make_shared<TraceScript>();
        doomed->attach_script(orphan);
        survivor = orphan;
        weak_node = doomed;
        check(orphan->get_owner() == doomed, "el script referencia a su nodo mientras vive");
    }
    check(weak_node.expired(), "el nodo se destruye aunque el script lo referencie");
    check(survivor->get_owner() == nullptr, "get_owner queda vacio tras morir el nodo");
}

static void test_prefabs_script_logic() {
    std::cout << "pui logica\n";

    // -- h_slider: el handle sigue al raton en x, dentro de [line, line+limit].
    auto h_container = std::make_shared<Node2D>("hc", Vec2{0, 0});
    auto h_offset = std::make_shared<int>(100);
    auto h_line = pui::h_slider(h_container, h_offset, 30, Color::WHITE, Color::BLACK, Color::BLACK, Color::WHITE);
    h_container->add_child(h_line);
    auto h_slider = std::dynamic_pointer_cast<NodeButton>(h_line->get_child(0));
    auto h_drag = h_slider->get_script_as<pui::detail::AxisSlider>();
    check(h_drag != nullptr, "h_slider monta un AxisSlider en el handle");
    check(h_drag->axis == pui::detail::AxisSlider::Axis::Horizontal, "h_slider usa el eje horizontal");
    check(h_drag->limit == 30 - 7, "h_slider mantiene su limite original (size - 7)");

    // El script mueve el handle y despues el container, asi que la posicion global
    // del handle vuelve a shifting al final del frame. Se comprueba la local, que
    // es estable, y el container por separado.
    auto reset_h = [&]{
        h_container->set_global_position({0, 0});
        h_slider->set_global_position({0, 0});
        h_drag->moving = false;
    };

    Input::mouse_pos = {10, 0};
    Input::l_click = true;
    Input::l_click_just_pressed = true;
    h_drag->on_process(0.016);
    check(h_slider->get_local_position() == (Vec2{10, 0}), "h_slider lleva el handle al raton");
    // progress = 10/23, container = -(int)(0.4347 * 100) = -43
    check(h_container->get_global_position() == (Vec2{-43, 0}),
          "h_slider desplaza el container segun el progreso");

    // Fuera del eje (una fila mas abajo) no hay hit test.
    reset_h();
    Input::mouse_pos = {10, 5};
    Input::l_click_just_pressed = true;
    h_drag->on_process(0.016);
    check(!h_drag->moving, "h_slider ignora el raton fuera de su fila");

    // El handle no puede pasar del limite.
    reset_h();
    Input::mouse_pos = {28, 0};
    Input::l_click = true;
    Input::l_click_just_pressed = true;
    h_drag->on_process(0.016);
    check(h_slider->get_local_position() == (Vec2{23, 0}), "h_slider respeta el limite maximo");

    // El handle tampoco puede quedar antes del origen.
    reset_h();
    Input::mouse_pos = {-9, 0};
    h_drag->on_process(0.016);
    check(h_slider->get_local_position() == (Vec2{0, 0}), "h_slider respeta el limite minimo");

    // -- v_slider: mismo script, eje vertical.
    auto v_container = std::make_shared<Node2D>("vc", Vec2{0, 0});
    auto v_offset = std::make_shared<int>(100);
    auto v_line = pui::v_slider(v_container, v_offset, 30, Color::WHITE, Color::BLACK, Color::BLACK, Color::WHITE);
    v_container->add_child(v_line);
    auto v_slider = std::dynamic_pointer_cast<NodeButton>(v_line->get_child(0));
    auto v_drag = v_slider->get_script_as<pui::detail::AxisSlider>();
    check(v_drag != nullptr, "v_slider monta un AxisSlider en el handle");
    check(v_drag->axis == pui::detail::AxisSlider::Axis::Vertical, "v_slider usa el eje vertical");
    check(v_drag->limit == 30 - 4, "v_slider mantiene su limite original (size - 4)");

    auto reset_v = [&]{
        v_container->set_global_position({0, 0});
        v_slider->set_global_position({0, 0});
        v_drag->moving = false;
    };

    Input::mouse_pos = {0, 10};
    Input::l_click = true;
    Input::l_click_just_pressed = true;
    v_drag->on_process(0.016);
    check(v_slider->get_local_position() == (Vec2{0, 10}), "v_slider lleva el handle al raton");
    // progress = 10/26, container = -(int)(0.3846 * 100) = -38
    check(v_container->get_global_position() == (Vec2{0, -38}),
          "v_slider desplaza el container en el eje correcto");

    // El eje horizontal de v_slider no debe moverse.
    check(v_container->get_global_position().x == 0, "v_slider no toca el eje perpendicular");

    // Fuera de su columna no hay hit test.
    reset_v();
    Input::mouse_pos = {3, 10};
    Input::l_click_just_pressed = true;
    v_drag->on_process(0.016);
    check(!v_drag->moving, "v_slider ignora el raton fuera de su columna");

    // Limite vertical: 30 - 4 = 26.
    reset_v();
    Input::mouse_pos = {0, 28};
    Input::l_click = true;
    Input::l_click_just_pressed = true;
    v_drag->on_process(0.016);
    check(v_slider->get_local_position() == (Vec2{0, 26}), "v_slider respeta su limite vertical");

    // -- confirm_dialog: arrastre desde la barra de titulo.
    auto dialog = pui::confirm_dialog("Hola", [](){});
    auto title = std::dynamic_pointer_cast<NodeButton>(dialog->get_child(0));
    auto drag = title->get_script_as<pui::detail::DragToMove>();
    check(drag != nullptr, "confirm_dialog monta un DragToMove en el titulo");

    Input::mouse_pos = {25, 10};
    drag->begin_drag();
    check(drag->moving, "begin_drag arranca el arrastre");
    check(drag->grab_offset == (Vec2{5, 5}), "begin_drag guarda el offset de agarre");

    Input::mouse_pos = {26, 11};
    Input::l_click = true;
    drag->on_process(0.016);
    check(dialog->get_global_position() == (Vec2{31, 16}), "el arrastre mueve el dialogo");

    // Soltar el raton aplica la posicion final ese frame y luego para: es el
    // comportamiento original, que el test fija para que no cambie por casual.
    Input::mouse_pos = {27, 12};
    Input::l_click = false;
    drag->on_process(0.016);
    check(dialog->get_global_position() == (Vec2{32, 17}), "el frame de soltado aplica la posicion final");
    check(!drag->moving, "soltar el raton detiene el arrastre");
    Input::mouse_pos = {40, 40};
    drag->on_process(0.016);
    check(dialog->get_global_position() == (Vec2{32, 17}), "ya no se mueve tras soltar");

    Input::l_click = false;
    Input::l_click_just_pressed = false;
}

int main() {
    std::cout << "GuayabaTUIEngine - test de API\n\n";

    MockTerminal mock;
    test_vec2();
    test_color();
    test_renderer(mock);
    test_node_tree();
    test_node2d_and_cache();
    test_ui_nodes(mock);
    test_input_delegation();
    test_scene_manager();
    test_pui_prefabs();
    test_script(mock);
    test_prefabs_script_logic();

    std::cout << "\n";
    if (failures == 0) {
        std::cout << "Todos los checks pasaron.\n";
        return 0;
    }
    std::cout << failures << " check(s) fallaron.\n";
    return 1;
}