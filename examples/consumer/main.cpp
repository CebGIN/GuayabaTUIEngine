/*
    Demo interactiva de GuayabaTUIEngine.

        #include <guayaba/Guayaba.hpp>
        using namespace guayaba;

    Patron recomendado por el motor:
    - Crear funciones que se encarguen de construir las escenas.
    - Las funciones devuelven std::unique_ptr<Node>: la funcion es la duena del
      nodo hasta que lo entrega, y el arbol es dueno a partir de ahi.
    - En el main():
    - Crear la root como root = crearEscena();
    - Establecer la root como la escena activa con
      SceneManager::get_instance().change_scene(std::move(root));
    - Iniciar el bucle de ejecucion con SceneManager::get_instance().start_running();
      Al iniciar el bucle la ejecucion del main se mantendra en ese punto,
      cualquier logica adicional debe ser manejada por el contenido de la escena.

    Esta demo ademas enseña señales: como un nodo avisa a otro sin que ninguno
    guarde un puntero al otro, y como una conexion desaparece sola cuando el nodo
    que la creo muere.
*/

#include <guayaba/Guayaba.hpp>

using namespace guayaba;

/*
    Emisor. Declara su señal y la dispara; no sabe -ni necesita saber- quien la
    escucha, ni guarda referencia alguna a quien lo escucha.
*/
class Enemigo : public Script<Node2D> {
public:
    int vida = 3;

    void on_attached() override {
        // Opcional: emit_signal la declara sola. Se declara aqui para que la
        // firma sea legible antes de que nadie emita nada.
        declare_signal<int>("danio");
    }

    void recibirGolpe(int d) {
        vida -= d;
        emit_signal("danio", d);
    }
};

/*
    Receptor, y a la vez oyente de dos señales del motor.

    Todo se conecta en on_attached, que es el hook de construccion: el subarbol ya
    esta completo y este nodo ya tiene dueña. Cada conexion queda registrada a
    nombre de ESTE script, asi que las tres desaparecen cuando este nodo muera.
    No hay ids que guardar ni desconexiones que escribir.
*/
class Interfaz : public Script<Node> {
public:
    Interfaz(NodeRef<Node2D> enemigo, NodeRef<NodeButton> boton_salir) :
        enemigo(std::move(enemigo)), boton_salir(std::move(boton_salir)) {}

    void on_attached() override {
        // Una señal propia de otro nodo. Los argumentos salen de la firma del
        // metodo, asi que no hay lambda capturando `this` ni tipos repetidos.
        if (Node2D* objetivo = enemigo.get()) {
            objetivo->connect_script_signal("danio", this, &Interfaz::al_recibir_danio);
        }

        // Una señal que el motor ya emite: NodeButton declara "clicked" en su
        // constructor y handle_click() la dispara antes de llamar al callback.
        if (NodeButton* boton = boton_salir.get()) {
            boton->connect_script_signal("clicked", this, &Interfaz::salir);
        }

        // Y una del bus global, sin dueña: la emite el terminal, no un nodo.
        bus().connect_script_signal("mouse_clicked", this, &Interfaz::al_hacer_click);
    }

    void al_recibir_danio(int d) { danio_total += d; }

    void salir() { SceneManager::get_instance().stop_running(); }

    void al_hacer_click(int boton, Vec2 posicion) { clicks++; }

    int danio_total = 0;
    int clicks = 0;

    // Los dos handles cruzan los limites del subarbol que las creo, asi que son
    // NodeRef: si el nodo ya no esta, get() responde nullptr en vez de quedar
    // colgando. attach_script reenvia sus argumentos al constructor del script.
    NodeRef<Node2D> enemigo;
    NodeRef<NodeButton> boton_salir;
};

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

    const NodeRef<Node2D> enemigo = contenido->add_child<Node2D>("enemigo", Vec2{2, 8});
    const NodeRef<NodeButton> salir = contenido->add_child<NodeButton>("salir", Vec2{40, 16},
        Color::BLACK, Color::BRIGHT_WHITE,
        std::vector<std::string>{
            ".--------.",
            "| Salir  |",
            "'--------'"});

    // La escena conecta lo suyo. El boton declara "clicked" solo, en su
    // constructor; este node la emite una vez para que se vea el camino completo.
    const ScriptRef<Node2D, Enemigo> guiones_enemigo = enemigo->attach_script<Enemigo>();
    contenido->attach_script<Interfaz>(enemigo, salir);

    // El emisor dispara cuando quiere, sin mirar quien escucha. Nadie tiene que
    // haberse registrado antes para que esto no falle, aunque en este caso si hay
    // alguien escuchando y por eso danio_total vale 1.
    if (Enemigo* e = guiones_enemigo.script()) e->recibirGolpe(1);

    // La via con callback sigue existiendo y no se sustituye por la señal.
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