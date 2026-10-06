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

    Y las cuatro maneras de enganchar un reactor a una señal, todas sobre la misma
    señal "danio" y todas visibles en el panel:

    - Por PUNTERO A METODO desde un Script: connect_script_signal. Los argumentos
      salen de la firma del metodo y el dueno lo pone el propio script.
    - Por LAMBDA: connect_signal<int>(...), con los argumentos escritos a mano
      porque no se pueden deducir de una lambda.
    - Por una FUNCION LIBRE, sin duena: la conexion no se descarta sola, asi que
      hay que cortarla a mano. No es eternal -vive en la tabla del emisor y muere
      con el-, lo que no hace es caerse cuando muere el oyente.
    - Con once = true, que se borra sola tras el primer disparo, frente a una
      conexion normal que se corta a mano con disconnect_signal. Las dos cuentan
      1 en el panel, y por eso mismo las dos existen.

    Las conexiones que tienen duena -casi todas- desaparecen solas cuando su
    duena muere: el id del nodo al que pertenece el reactor se guarda en la
    conexion, y el siguiente emit descarta las que ya no lo tienen. Ver
    Core/Signal.hpp.
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
    Las dos que se conectan sin duena, por debajo del guion que las cuenta.

    Una funcion libre no tiene ni this ni dueña de la que depender, asi que sus
    contadores son globales y sobreviven a la escena. Son globales por necesidad
    tecnica y no por gusto: es lo unico que una funcion libre puede tocar. Por eso
    su linea del panel va aparte de las demas, y por eso no puede ni repintar el
    panel ella misma - el repintado final lo hace el reactor que se conecto el
    ultimo, que si tiene this.
*/
static int veces_en_funcion_libre = 0;
static int veces_una_sola_vez = 0;

static void contar_en_funcion_libre(int) { ++veces_en_funcion_libre; }

/*
    Receptor, y a la vez oyente de las señales que hay en escena.

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
        // 1. Por puntero a metodo, a una señal propia de otro nodo. Los argumentos
        //    salen de la firma del metodo, asi que no hay lambda capturando `this`
        //    ni tipos repetidos, y el dueno lo pone este script y no el emisor.
        if (NodeSQ* emisor = enemigo.get()) {
            emisor->connect_script_signal("danio", this, &Interfaz::al_recibir_danio);
            conectar_los_demas(emisor);
        }

        // 2. "clicked", que NodeButton declara en su constructor y dispara en
        //    handle_click(). Es la unica via que tiene un boton: antes tambien
        //    aceptaba un set_on_click, y entonces un boton solo podia atender a
        //    un oyente mientras que esta senal atiende a cuantos haga falta.
        //
        //    El boton se busca por ruta solo para conectarlo. El handle no se
        //    guarda y no hace falta para nada mas, y tampoco hay que acordarse
        //    de desconectarlo cuando el boton se vaya: el dueno lo pone este
        //    script, asi que la conexion muere con el.
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

    // Las tres formas menos obvias, sobre la MISMA señal que la de arriba: si
    // alguma de estas no contase, el panel lo diria.
    void conectar_los_demas(NodeSQ* emisor) {
        // 2. Por lambda. connect_signal<int>(...) y no connect_signal(...): los
        //    argumentos no se pueden deducir de una lambda, hay que escribirlos.
        //
        //    Y se conecta en el EMISOR, no en el nodo de este script. El
        //    connect_signal de ScriptBase es para la señal del propio nodo, y usa
        //    signal<>() en vez de declare_signal<>(): si este nodo no declara
        //    "danio" -y no lo declara, porque declararla seria inventarse un
        //    evento que nadie emite- entonces lanza en vez de crear en silencio
        //    una señal que no se dispararia nunca. Por eso va aqui y no dentro.
        //
        //    owner = node: el dueno es este nodo, no el emisor, asi que la lambda
        //    desaparece con el oyente y no cuando muera el emisor.
        emisor->connect_signal<int>("danio", [this](int d) {
            ++vistos_por_lambda;
            dano_lambda += d;
            pintar_panel();
        }, /*once=*/false, /*owner=*/node);

        // 3. Una funcion libre, sin ninguna duena. No es un metodo de este script
        //    ni una lambda que capture este nodo, asi que connect no tiene de quien
        //    leer un dueno y la conexion no se descarta sola.
        //
        //    Ojo con lo que eso NO significa: no significa que sobreviva para
        //    siempre. La conexion vive en la tabla de señales del EMISOR, asi que
        //    muere con el. Lo que no se descarta es por su cuenta cuando este
        //    nodo muera, que es justo lo que haria una conexion con dueno.
        //
        //    La diferencia sale al conectar la funcion suelta a una señal del BUS:
        //    ahi si no hay duena posible y si hay que cortarla a mano, porque el
        //    bus vive mas que cualquier escena.
        emisor->connect_signal<int>("danio", &contar_en_funcion_libre);

        // 4a. once = true: esta conexion se borra sola en cuanto se llama. El
        //     contador "una vez" se queda en 1 para siempre y las demas siguen
        //     subiendo, que es justo la diferencia que hay que ver.
        emisor->connect_signal<int>("danio", [](int) {
            ++veces_una_sola_vez;
        }, /*once=*/true, /*owner=*/node);

        // 4b. Y esta se corta a mano, para que el panel muestre las dos vias de
        //     parar un reactor haciendo lo mismo. connect devuelve el id, que es
        //     lo unico que despues permite desconectar: sin ese id no hay forma de
        //     dejar de oir. Se guarda porque el corte ocurre mas tarde, dentro de
        //     un reactor, y no en la linea que conecto.
        id_a_cortar = emisor->connect_script_signal("danio", this, &Interfaz::cortar_la_conexion);

        // Consulta, no excepcion: false si el id no existe, en vez de lanzar.
        if (!emisor->is_connected(id_a_cortar)) throw std::logic_error("demo: id recien creado");

        // 5. Y al final, un reactor que solo repinta. No es adorno: sin el, el
        //    panel miente.
        //
        //    El orden de conexion es el orden de ejecucion, asi que un reactor
        //    que repinta en medio ve el estado anterior a los que se conectaron
        //    despues. La funcion libre no puede ni intentar pintar -no tiene
        //    this-, y "once" y el corte se contaban a si mismos despues de haber
        //    pintado. Con esto al final, estas lineas ya ven todas las cuentas
        //    al dia.
        emisor->connect_signal<int>("danio", [this](int) { pintar_panel(); },
                                    /*once=*/false, /*owner=*/node);
    }

    // El corte manual. Llega por la misma señal que el resto, asi que en el panel
    // se ve igual que cualquier otra cosa: por aqui solo pasa una vez.
    void cortar_la_conexion(int) {
        ++veces_hasta_cortar;
        if (veces_hasta_cortar < 2) return;

        // disconnect por id. A partir de aqui la señal ya no llama a este metodo,
        // y el panel lo dice en la linea del corte.
        if (enemigo.get() && enemigo->disconnect_signal(id_a_cortar)) ++cortes;
        pintar_panel();
    }

    // La carga de la señal propia. Un int y nada mas: ni quien emite ni quien
    // escucha sabe nada del otro mas alla de este nombre.
    void al_recibir_danio(int d) { dano_total += d; ++envios; pintar_panel(); }


    // Cuenta, y para. El boton no necesita saber quien lo pulsa ni tener un
    // callback propio: este script se engancho a "clicked" y ya esta avisado.
    void al_pulsar_el_boton() {
        ++pulsaciones;
        pintar_panel();
        SceneManager::get_instance().stop_running();
    }

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

    // Lo de las tres formas de engancharse que no son puntero a metodo.
    int vistos_por_lambda = 0;
    int dano_lambda = 0;
    int veces_hasta_cortar = 0;
    int cortes = 0;
    uint64_t id_a_cortar = 0;

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
            // Ancho: la caja "fondo" mide 64 columnas y el panel empieza en la 12,
            // asi que ninguna linea puede pasar de 52. Las etiquetas se recortan a
            // proposito -caben, y lo que explica cada una va en el guion-, porque
            // una linea que se desborda no se corta: se come la de al lado.
            ui->set_text({
                "SENAL danio: " + std::to_string(envios) + " envios, "
                    + std::to_string(dano_total) + " de dano",
                "  metodo   (dueño = este guion) : " + std::to_string(envios),
                "  lambda   (connect_signal<int>) : " + std::to_string(vistos_por_lambda),
                "  libre    (sin dueño)          : " + std::to_string(veces_en_funcion_libre),
                "  once     (se borra sola)      : " + std::to_string(veces_una_sola_vez),
                "  corte    (disconnect a mano)  : " + std::to_string(veces_hasta_cortar)
                    + ", " + std::to_string(cortes) + " cortes",
                "click en el boton: " + std::to_string(pulsaciones)
                    + "   (senal \"clicked\")",
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

    // La pista va DEBAJO del panel y ARRIBA del boton, con las filas contadas
    // para que nada se pise. Un NodeUI que se solapa no se recorta: escribe
    // encima de lo que tenga debajo, asi que el orden de las filas importa.
    //   fondo en (0,0), contenido en (4,5)
    //   panel  -> columnas 12..60, filas 5..13   (nueve lineas)
    //   pista  -> columnas  4..   , filas 15,16 (dos lineas, cortas a proposito)
    //   salir  -> columnas 40..49, filas 17..19
    contenido->add_child<NodeUI>("pista", Vec2{0, 10},
        Color::BRIGHT_CYAN, Color::BLUE,
        std::vector<std::string>{
            "clic fuera del boton -> lo ve el bus (1=izq, 2=der)",
            "clic dentro de [ Salir ] -> lo ve su senal clicked"});

    // El boton no necesita handle aqui. Se engancha por su cuenta desde el guion
    // de abajo, que lo busca por ruta, asi que la escena ni lo guarda ni lo
    // conecta: no hay set_on_click, ni un lambda que mantener, ni que saber quien
    // va a escucharlo. El guion se engancha antes de que este nodel exista ya en
    // el arbol porque attach_script va despues, asi que el orden de estas lineas
    // es el que hace que get_node_or_null lo encuentre.
    contenido->add_child<NodeButton>("salir", Vec2{36, 12},
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

    return root;
}

int main() {
    std::unique_ptr<Node> root = crearEscena();

    SceneManager::get_instance().change_scene(std::move(root));
    SceneManager::get_instance().start_running();

    return 0;
}