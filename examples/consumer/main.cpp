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

    Esta demo enseña el sistema de señales viéndolo funcionar, no leyendolo:

    - El recuadro de la izquierda parpadea solo cada 0.6 s. Ese nodo declara la
      señal "danio" y la dispara sin preguntar a nadie quien la escucha, y sin
      mirar siquiera si hay alguien escuchando.
    - El panel reescribe sus lineas desde el script que escucha, asi que los
      contadores son la prueba visible de que las conexiones estan vivas.
    - Un clic en cualquier sitio suma en la linea del bus, porque "mouse_clicked"
      y "mouse_moved" los emite el terminal y no un nodo: no hay ningun boton
      que haya que señalar.
    - Un clic dentro de [ Salir ] suma solo en su linea. Ahi lo que se ha
      disparado es la señal "clicked" del boton, y el bus lo ve igual que a
      cualquier otro clic.

    Ninguna conexion de esta demo se desconecta. Cada una guarda el id del nodo
    al que pertenece el reactor -el nodo desde el que se conecto-, y cuando ese
    nodo muere el reactor se descarta en el siguiente emit. Ver Core/Signal.hpp.
*/

#include <guayaba/Guayaba.hpp>

using namespace guayaba;

/*
    Emisor. Declara su señal y la dispara; no sabe -ni necesita saber- quien la
    escucha, ni guarda referencia alguna a quien lo escucha.
*/
class Enemigo : public Script<NodeSQ> {
public:
    void on_attached() override {
        // Opcional: emit_signal la declara sola. Se declara aqui para que la
        // firma sea legible antes de que nadie emita nada, y para que
        // has_signal<int>("danio") sea una respuesta de verdad desde el principio.
        declare_signal<int>("danio");
    }

    void on_process(double delta_time) override {
        // El reloj es lo unico que decide cuando emitir. El emisor no consulta a
        // nadie, no lleva la cuenta de quien le escucha y no depende de que haya
        // alguien mirando: dispara cuando le toca y quien este conectado se
        // entera. on_process corre en cada frame de este nodo.
        reloj += delta_time;
        if (reloj < periodo) return;

        reloj = 0.0;
        recibirGolpe(1 + golpes % 2);   // alterna 1 y 2, para que se note la carga
    }

    void recibirGolpe(int d) {
        ++golpes;
        emit_signal("danio", d);

        // El parpadeo es adorno, y va por el nodo y no por la señal: es el
        // emisor quien cambia de color, sin que el oyente se lo pida. Cambia el
        // texto del panel es lo que hace el oyente, con lo que el evento le dio.
        node->change_text_color(golpes % 2 == 0 ? Color::BRIGHT_YELLOW : Color::BRIGHT_WHITE);
    }

    double periodo = 0.6;
    double reloj = 0.0;
    int golpes = 0;
};

/*
    Receptor, y a la vez oyente de las cuatro señales que hay en escena.

    Todo se conecta en on_attached, que es el hook de construccion: el subarbol ya
    esta completo y este nodo ya tiene dueña. Cada conexion queda registrada a
    nombre de ESTE script, asi que las cuatro desaparecen cuando este nodo muera.
    No hay ids que guardar ni desconexiones que escribir.
*/
class Interfaz : public Script<Node2D> {
public:
    // attach_script reenvia sus argumentos al constructor del script, asi que el
    // destino llega como cualquier otro argumento de construccion.
    Interfaz(NodeRef<NodeSQ> enemigo, NodeRef<NodeUI> panel) :
        enemigo(std::move(enemigo)), panel(std::move(panel)) {}

    void on_attached() override {
        // 1. Una señal propia de otro nodo. Los argumentos salen de la firma del
        //    metodo, asi que no hay lambda capturando `this` ni tipos repetidos.
        if (NodeSQ* emisor = enemigo.get()) {
            emisor->connect_script_signal("danio", this, &Interfaz::al_recibir_danio);
        }

        // 2. "clicked", que NodeButton declara en su constructor y dispara en
        //    handle_click(), antes de su devolucion de llamada de siempre. El
        //    boton se busca por ruta solo para conectarlo: el handle no se
        //    guarda y no hace falta para nada mas, que es justo lo que una
        //    devolucion de llamada no puede prometer - registrarla obliga a
        //    llegar hasta el boton, y desregistrarla, a acordarse de
        //    desconectarla cuando ese boton se vaya.
        if (NodeButton* boton = node->get_node_or_null<NodeButton>("salir").get()) {
            // has_signal es una consulta, no una excepcion: false en vez de
            // lanzar si el nombre no existe o si existe con otra firma.
            if (boton->has_signal("clicked")) {
                boton->connect_script_signal("clicked", this, &Interfaz::al_pulsar_el_boton);
            }
        }

        // 3. El bus global, que no tiene dueña: lo emite el terminal desde
        //    Input::refresh_input(), que el bucle llama antes de actualizar los
        //    scripts, asi que un reactor ve el clic en el mismo frame. Aqui los
        //    argumentos tambien salen de la firma del metodo, y el dueno lo pone
        //    el propio script, asi que estas dos conexiones mueren con este nodo.
        bus().connect_script_signal("mouse_clicked", this, &Interfaz::al_hacer_click);
        bus().connect_script_signal("mouse_moved", this, &Interfaz::al_mover_el_raton);

        pintar_panel();
    }

    // La carga de la señal propia. Un int y nada mas: ni quien emite ni quien
    // escucha sabe nada del otro mas alla de este nombre.
    void al_recibir_danio(int d) { dano_total += d; ++envios; pintar_panel(); }

    // Solo cuenta. El boton conserva ademas su devolucion de llamada de toda la
    // vida (set_on_click, mas abajo) y las dos vias se disparan juntas: la señal
    // demuestra que otro script puede reaccionar sin tocar el boton, y el cierre
    // del bucle se deja en el callback, que es lo que ya habia.
    void al_pulsar_el_boton() { ++pulsaciones; pintar_panel(); }

    // El boton del bus es un int a proposito (1 = izquierdo, 2 = derecho) para
    // que un reactor pueda compararlo sin incluir la cabecera del terminal.
    void al_hacer_click(int b, Vec2 posicion) {
        ++clicks;
        ultimo_boton = b;
        ultima_posicion = posicion;
        pintar_panel();
    }

    void al_mover_el_raton(Vec2 posicion) { raton = posicion; pintar_panel(); }

    // Los dos handles cruzan los limites del subarbol que las creo, asi que son
    // NodeRef: si el nodo ya no esta, get() responde nullptr en vez de quedar
    // colgando.
    NodeRef<NodeSQ> enemigo;
    NodeRef<NodeUI> panel;

    int envios = 0;
    int dano_total = 0;
    int pulsaciones = 0;
    int clicks = 0;
    int ultimo_boton = 0;
    Vec2 ultima_posicion = {0, 0};
    Vec2 raton = {0, 0};

    std::string nombre_del_boton() const {
        if (ultimo_boton == 1) return "izq";
        if (ultimo_boton == 2) return "der";
        return "-";
    }

    // El panel es un NodeUI corriente: un set_text y en el siguiente frame ya se
    // ve. Es la unica prueba que hace falta de que lo de arriba esta vivo, y no
    // hace falta ni un cronometro ni un flag: se repinta desde el mismo reactor
    // que acaba de cambiar el estado.
    void pintar_panel() {
        if (NodeUI* ui = panel.get()) {
            ui->set_text({
                "senal danio: " + std::to_string(envios) + " envios, "
                    + std::to_string(dano_total) + " de dano",
                "click en el boton: " + std::to_string(pulsaciones),
                "click en el bus: " + std::to_string(clicks) + ", ultimo "
                    + nombre_del_boton() + " en "
                    + std::to_string(ultima_posicion.x) + ","
                    + std::to_string(ultima_posicion.y),
                "raton en " + std::to_string(raton.x) + "," + std::to_string(raton.y),
            });
        }
    }
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

    // Este nodo no dibuja nada: existe para llevar el script que escucha, y para
    // que quede claro que el que reacciona no es el nodo que emite.
    const NodeRef<Node2D> contenido = fondo->add_child<Node2D>("contenido", Vec2{4, 5});

    // El emisor es un recuadro vacio: solo tiene que tener una señal que alguien
    // escuche, y verse cuando la dispara.
    const NodeRef<NodeSQ> enemigo = contenido->add_child<NodeSQ>("enemigo", Vec2{2, 0},
        Vec2{5, 3}, Color::BRIGHT_WHITE, Color::BLUE);

    const NodeRef<NodeUI> panel = contenido->add_child<NodeUI>("panel", Vec2{8, 0},
        Color::BRIGHT_GREEN, Color::BLUE, std::vector<std::string>{"panel vacio"});

    contenido->add_child<NodeUI>("pista", Vec2{0, 4},
        Color::BRIGHT_CYAN, Color::BLUE,
        std::vector<std::string>{
            "clic en cualquier sitio -> lo ve el bus (1=izq, 2=der)",
            "clic dentro de [ Salir ] -> ademas lo ve su propia senal"});

    const NodeRef<NodeButton> salir = contenido->add_child<NodeButton>("salir", Vec2{36, 11},
        Color::BLACK, Color::BRIGHT_WHITE,
        std::vector<std::string>{
            ".--------.",
            "| Salir  |",
            "'--------'"});

    // La escena conecta lo suyo. El guion del emisor declara "danio" en su
    // on_attached y el del panel lo escucha desde el suyo: ninguno de los dos
    // necesita un puntero al otro, solo el nombre de la señal y el orden en que
    // se adjuntan.
    const ScriptRef<NodeSQ, Enemigo> guion_enemigo = enemigo->attach_script<Enemigo>();
    contenido->attach_script<Interfaz>(enemigo, panel);

    // ScriptRef re-deriva el guion a partir del id de su nodo, asi que da igual
    // cuando se consulta: si el nodo ya no esta, script() responde nullptr.
    if (Enemigo* e = guion_enemigo.script()) e->periodo = 0.6;

    // El boton declara "clicked" solo, en su constructor. Esta via de callback
    // sigue existiendo y no se sustituye por la señal: las dos se disparan.
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