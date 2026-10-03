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

    int enters = 0, exits = 0, processes = 0;
    child->set_at_enter_function([&enters]{ ++enters; });
    child->set_at_exit_function([&exits]{ ++exits; });
    child->set_process_function([&processes](double){ ++processes; });
    root->update(0.016);
    check(processes == 1, "update llama al process function");

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

    std::cout << "\n";
    if (failures == 0) {
        std::cout << "Todos los checks pasaron.\n";
        return 0;
    }
    std::cout << failures << " check(s) fallaron.\n";
    return 1;
}