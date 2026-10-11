# GuayabaTUIEngine

Lightweight pure C++17 engine for building console user interfaces, with mouse interaction, cross-compatible between Windows and Linux.

## Uso

```cpp
#include <guayaba/Guayaba.hpp>
using namespace guayaba;

std::unique_ptr<Node> crearEscena() {
    auto root = std::make_unique<Node>("root");

    NodeRef<Control> box = root->add_child<Control>("box", Vec2{0, 0});
    box->set_anchor_preset(AnchorPreset::FullRect);
    box->set_clip_children(true);

    auto label = box->add_child<NodeTextMatrix>("label");
    label->set_text({"Hola"});
    label->set_local_position({2, 2});

    return root;
}

int main() {
    SceneManager::get_instance().change_scene(crearEscena());
    SceneManager::get_instance().start_running();
    return 0;
}
```

`start_running()` no devuelve hasta que alguien llama a `SceneManager::get_instance().stop_running()`.

Patron recomendado:
- Crear funciones que se encargan de construir las escenas.
- Las funciones devuelven `std::unique_ptr<Node>`: la funcion es la duena del nodo
  hasta que lo entrega, y el arbol es dueno a partir de ahi.
- En el `main()`: crear la root, entregarla con `change_scene()`, y arrancar el
  bucle con `start_running()`.

## Quien es duena de cada nodo

El arbol es duena de sus hijos sin condiciones: no hay refcounting en el arbol, asi
que un ciclo de referencias no es solo que queda desaconsejado, es que no se puede
escribir. La regla es una sola:

> Un nodo pertenece a quien lo creo hasta que lo entrega, y al arbol desde ese
> momento.

```cpp
auto root = std::make_unique<Node>("root");       // tu lo duenes
NodeRef<NodeTextMatrix> titulo = root->add_child<NodeTextMatrix>(...); // el arbol ya lo duene
SceneManager::get_instance().change_scene(std::move(root));
```

Nada que nombre un nodo devuelve un puntero crudo. Toda referencia es un
`NodeRef<T>`, que guarda un id y no una direccion: cuando el nodo desaparece,
responde vacio en vez de quedar colgando. Para llegar al nodo, `get()` y
comprueba el resultado.

Un nodo se nombra de tres formas, y ninguna obliga a recorrer el arbol para ser
segura:

| Pregunta | Como se responde |
|---|---|
| ¿donde esta? | `child_index()` |
| ¿de quien depende? | `get_path()`, `get_node<T>()`, `is_ancestor_of()` |
| ¿sigue existiendo? | `NodeRef<T>` |

### Tabla de operaciones

| Operacion | Quien queda como dueno |
|---|---|
| `add_child<T>(...)` / `add_child(std::move(p))` | el arbol |
| `remove_child(idx)` | tu, mediante el `unique_ptr` que devuelve |
| `change_scene(std::move(root))` | el `SceneManager` |
| `take_scene()` | tu, otra vez |
| un nodo que nunca se inserto | tu |

`remove_child()` recibe el **indice** del hijo, el que te da `child_index()`, y
devuelve `std::unique_ptr<Node>`: no destruye nada. Si no te quedas con el
`unique_ptr` que devuelve, el subarbol se va con el. No existe la fuga de "quite
el nodo del arbol y me olvide de liberarlo" que si admite un `remove_child` que
devuelve `void`. Un indice fuera de rango lanza `std::out_of_range` en vez de
devolver `nullptr`.

No existe `reparent()`. Mover un nodo son las dos mitades a la vista, que es justo
lo que deja claro quien es dueno en cada momento:

```cpp
std::unique_ptr<Node> propiedad = viejo->remove_child(nodo->child_index());
nuevo->add_child(std::move(propiedad));
```

Si lo que tienes es un `NodeRef` al hijo y no al padre, `steal()` hace lo mismo
desde el lado del nodo, sin subir al padre y volver a bajar al indice:

```cpp
std::unique_ptr<Node> propiedad = ref.get()->steal();
```

Un nodo raiz no se puede robar: su dueno es quien lo creo, o el `SceneManager`, y
ninguno de los dos se ve desde aqui. `steal()` lanza `std::logic_error` en ese
caso, y tambien en un nodo que ya esta suelto.

Un detalle: `remove_child` seguido de `add_child` sobre el **mismo** padre mueve
el nodo al final, lo que cambia su orden de dibujo. `reparent()` hacia el mismo
padre era un no-op; ahora hay que comprobarlo antes, porque anadir al final es
justo lo que hace `add_child`.

Un nodo raiz no puede liberarse a si mismo: no tiene padre, asi que el arbol no lo
duena. Para eso esta `take_scene()`, o `change_scene(nullptr)`.

### `free()` y `queue_free()`

Ambos **requieren el bucle corriendo**: solo preparan la destruccion, y el
`SceneManager` la ejecuta al final del frame. Llamarlos fuera del bucle deja el
nodo pendiente, sin destruir.

- `free()` desparenta al instante (deja el arbol y deja de dibujarse) y se destruye
  en el vaciado siguiente. Se puede llamar desde un callback.
- `queue_free()` sigue parenteado: se sigue actualizando y dibujando hasta el
  vaciado, igual que en Godot.

Desparentar un nodo **durante** la iteracion esta permitido. El bucle recorre los
hijos por indice y vuelve a comprobar la ranura despues de cada llamada, asi que un
callback puede anadir o quitar nodos sin corromper el recorrido.

Un `queue_free()` sobre un nodo ya encolado no hace nada, y encolar un subarbol
cancela las entradas de sus descendientes ya encolados: cada nodo se destruye
exactamente una vez.

## Buscar nodos

Las rutas se resuelven contra el arbol, no se guardan, asi que sobreviven a mover
un nodo de sitio:

```cpp
root->get_node<NodeTextMatrix>("fondo/titulo");       // relativa
root->get_node<NodeTextMatrix>("/fondo/titulo");      // absoluta, incluye la raiz
root->get_node_or_null<NodeTextMatrix>("quizas");     // vacio si no existe
node->get_node<NodeTextMatrix>("../label");           // "." y ".." tambien
```

`get_node()` lanza excepcion si no encuentra el nodo; `get_node_or_null()` devuelve
un `NodeRef` vacio. La primera segmento de una ruta absoluta nombra a la raiz, de
modo que `get_node(get_path())` siempre funciona.

Los nombres de hermanos se hacen unicos: si anades dos hijos llamados `box`, el
segundo queda como `box@2`. Sin eso las rutas serian ambiguas. Un nombre no puede
contener `/`, porque las rutas se comparan como texto.

### Rutas en vez de punteros

Dos operaciones comparan relaciones, y ambas trabajan sobre la ruta en vez de un
nodo:

```cpp
// ¿esto ya esta por debajo de aquello?
hijo->is_ancestor_of(padre->get_path());   // lanza si cerraria un ciclo
padre->get_path_to(hijo->get_path());      // "/fondo/titulo"
```

El ventaja es que no hace falta conocer `NodeRef` para nada de esto, y que
`get_path_to()` tambien responde por un nodo ya destruido, porque no hay puntero
que desreferenciar. La contrapartida es que las rutas son **de un arbol**: dos
arboles distintos pueden contener cada uno `/root/a`. En `is_ancestor_of()` eso
puede dar un falso positivo, que `add_child()` convierte en una adicion rechazada;
nunca deja pasar un ciclo real.

Para referencias que cruzan subarboles, `NodeRef<T>`:

```cpp
class Seguidor : public Script<Node2D> {
public:
    // El objetivo cruza de subarbol, asi que llega por el constructor: un
    // NodeRef no se puede re-apuntar despues.
    explicit Seguidor(Node2D* objetivo) : objetivo(objetivo) {}
    void on_process(double) override {
        if (auto o = objetivo.get()) o->set_local_position(Input::mouse_pos);
    }
    NodeRef<Node2D> objetivo;   // vive en el subarbol de otro
};
```

Son 8 bytes, no poseen nada, y `get()` devuelve `nullptr` en cuanto el nodo se
destruye, sin importar quantas veces se reutilice esa direccion. No detectan una
referencia que sigue siendo valida pero quedo obsoleta (por ejemplo, tras mover el
nodo a otro sitio): para eso esta `get_node_or_null()`.

Solo ensanchan: un `NodeRef<NodeTextMatrix>` se convierte implicitamente en
`NodeRef<Node>`, nunca al reves. Estrechar es una pregunta que hay que hacer con
`get()` o con `get_node<T>()`, que ademas comprueban.

Un `NodeRef<T>` nombra un nodo para siempre: se construye desde un puntero y no cambia
de objetivo. Por eso `attach_script` reenvia sus argumentos al script: el objetivo
llega como cualquier otro argumento de constructor.

Los constructores no castean y no comprueban: un `N*` ya es del tipo correcto, y el
id fija que objeto es. Por eso un `NodeRef` mal tipado es un error de compilacion, no
un chequeo en tiempo de ejecucion, y por eso no hay `watch()`. Si anades una captura
sin tipar, el `static_cast` de `get()` deja de ser correcto en silencio.

## Logica propia: Script

Un `Script` cuelga logica y estado de un `Node` existente **sin derivar de la clase
C++** de ese nodo. La idea es el `Script` de Godot: poder añadir comportamiento a
un prefab que no escribiste, sin reimplementar su dibujado.

Hay dos clases y se derivan de distintas. `ScriptBase` es lo que el nodo guarda y
lo que despacha, y solo conoce a su dueno como `Node*`. `Script<N>` es de lo que
tu derivas: dice de que tipo de nodo se trata y guarda la referencia ya tipada, de
modo que dentro de tus hooks tocas las propiedades directamente, sin castear nada.

```cpp
class Parpadeo : public guayaba::Script<guayaba::Node2D> {
public:
    float periodo = 0.5f;
    float transcurrido = 0.0f;

    void on_process(double dt) override {
        transcurrido += dt;
        if (transcurrido >= periodo) {
            transcurrido = 0.0f;
            node->invertir_colores();   // tipado: ni un cast
        }
    }
};

auto parpadeo = nodo->attach_script<Parpadeo>();
parpadeo.script()->periodo = 0.2f;   // accede a tu campo por tu propia referencia
```

Deriva siempre de `Script<N>`, nunca de `ScriptBase`: un script escrito contra
`ScriptBase` tiene que volver a castear para llegar a algo interesante, y
`attach_script<T>()` rechaza un `T` que no venga de la plantilla. Con `N` por defecto
(`Script<>`) el dueno se queda en `Node*`, que es lo que quieres cuando el tipo
concreto no te importa.

`attach_script<T>()` construye el script, lo liga y te devuelve un `ScriptRef<N, T>`;
los argumentos que pases van al constructor de `T`. La version sin plantilla acepta
un script ya construido y devuelve `void`, porque ahi no hay ningun `T` del lado del
llamante con el que tipar la referencia:

```cpp
nodo->attach_script(std::make_unique<Parpadeo>());   // sin referencia tipada
```

No necesitas ningun cast para pasar un `unique_ptr<Parpadeo>`: `unique_ptr<Derived>`
se convierte solo a `unique_ptr<ScriptBase>`.

Dentro de un hook, `node` es el nodo ya estrechado a `N`: no hay ni un bloqueo ni
un contador de referencias, porque el nodo es dueno de su script, asi que mientras
corre cualquiera de tus metodos el nodo esta vivo por construccion. Ese mismo
puntero narrowed una sola vez, al ligar, y no en cada acceso. `get_owner()` sigue
disponible y devuelve el `Node*`; `get_owner_as<T>()` el tipo concreto.

`get_script_as<T>()` es una **pregunta**: si el nodo no tiene un `T`, devuelve un
`ScriptRef` vacio, y eso es una respuesta correcta. Lo contrario es
`attach_script<T>()`, que declara que el script del nodo **es** un `T` y por eso
devuelve un `ScriptRef` valido a el.

Como el `ScriptRef` necesita nombrar `node_type`, `get_script_as<T>()` pide lo mismo
que `attach_script<T>()`: que `T` derive de `Script<N>`. Preguntar por un
`ScriptBase` a pelo no compila.

### El hueco queda sellado

Un nodo guarda el primer script que le des **hasta que se destruye**. No hay
`detach_script()`, y adjuntar un segundo script lanza `std::logic_error`:

```cpp
nodo->attach_script<Parpadeo>();
nodo->attach_script<Otro>();   // std::logic_error
```

La razon no esdogsmatica: cambiar el script de un nodo dejaba invalido cualquier
`T*` que alguien hubiera guardado, sin forma de enterarte. Como `attach_script` te
devuelve un `ScriptRef` y no un puntero crudo, ese hueco deja de existir; pero solo
si de verdad no hay forma de cambiarlo mas adelante. Monta el subarbol y adjunta sus
scripts antes de handing el nodo a una escena.

`ScriptRef<N, T>` son 8 bytes y **no guardan puntero**: guardan el id del nodo dueno y
rederivan el script en cada acceso. Asi hereda gratis las dos garantias de `NodeRef`,
incluido no poder apuntar por error a otro script construido despues en la misma
direccion. `.script()` y `.node()` no castean.

Los dos constructores lanzan. El de un argumento descubre que `T` tiene el dueno, y
falla si no lo tiene; el de dos recibe script y nodo por separado y **verifica que
se pertenezcan**, porque si no, `.script()` haria un cast de `AudioScript*` a
`Parpadeo*`. El `dynamic_cast` se paga una vez, al construir; nunca por acceso.

```cpp
class Vigia : public Script<Node2D> {
public:
    // attach_script<> reenvia sus argumentos a T, asi que el objetivo llega
    // como cualquier otro argumento de constructor.
    explicit Vigia(ScriptRef<Node2D, Parpadeo> p) : parpadeo(p) {}
    void on_process(double) override {
        if (auto* p = parpadeo.script()) p->periodo = 0.5f;
    }
    ScriptRef<Node2D, Parpadeo> parpadeo;
};
```

Y la referencia muere con el nodo, sin dejar nada que comprobar a mano:

```cpp
ScriptRef<Node2D, Parpadeo> ref;
{
    Node2D nodo("nodo");
    ref = nodo.attach_script<Parpadeo>();
    ref.is_valid();          // true
}
ref.script();               // nullptr: el nodo se destruyo
```

### Hooks disponibles

| Hook | Cuando salta |
|---|---|
| `on_attached` | al ligar el script a un nodo, siempre |
| `on_detached` | desde el destructor del nodo dueno, la ultima cosa que ve tu script |
| `on_enter_tree` / `on_exit_tree` | solo si el nodo entra o sale del arbol de escena |
| `on_process(dt)` | cada frame, con el nodo en el arbol |

Los dos pares son ciclos distintos y no se mezclan. Como el hueco esta sellado, ligar
solo ocurre al montar el arbol, asi que `on_attached` es un hook de construccion, no
algo que dispare el arbol en marcha. Y un script ligado a un nodo que ya esta vivo
nunca vera `on_enter_tree`: pidelo tu.

```cpp
void on_attached() override {
    if (node->is_in_tree()) on_enter_tree();
}
```

`on_detached` salta **antes** de que se destruya ningun miembro del nodo, asi que
`children` y `parent` siguen en pie y tu `node` se lee como un nodo coherente. No
liberes nada desde ahi: es lo ultimo que ocurre.

Un script **no dibuja**. Como se ve un nodo lo decide el propio nodo, no un script
suelto encima: si quieres dibujado propio, deriva el nodo y sobrescribe `draw()`, que
recibe el `clip` activo y lo pasa a sus hijos. Asi el clip lo respeta sin que nadie
tenga que acordarse de hacerlo.

### Notas

- Como maximo un script por nodo, y ya no se sustituye. Adjuntar `nullptr` es un
  no-op silencioso y **no** sella el hueco, asi que un `attach_script(nullptr)` no te
  quita la posibilidad de adjuntar despues.
- Un `Script<N>` ligado a algo que no es `N` lanza `std::invalid_argument` y el nodo se
  queda **sin** script y **sin** sellar, o sea que reintentas cuando quieras. Ojo: eso
  lo detecta el `dynamic_cast` de `bind_to` en tiempo de ejecucion, no el compilador,
  porque `attach_script<T>()` no conoce el tipo del nodo que lo llama.
- El sello se pone solo tras un `bind_to` correcto, que es lo que permite distinguir un
  rechazo (nodo intacto, reintentable) de un segundo adjunto (nodo ya ocupado).
- Ninguna de las dos clases es abstracta en el sentido del lenguaje: todos los hooks
  traen un default vacio, para no obligar a un script que solo dibuja a escribir un
  `on_process` de mentira. Lo que se exige es el contrato, y `attach_script<T>()`
  rechaza un `T` abstracto: un hook que no puedes sobreescribir es un hook al que no
  se despacha nada.
- La propiedad del script se traslada al nodo, asi que un script ya ligado no puede
  volver a entrar por aqui: no hay ninguna via de la API que lo devuelva. Si eres
  tu el que fabrica un `unique_ptr` sobre un puntero que ya tiene un nodo, eso ya
  es un doble dueno, y ningun guard en este metodo lo haria seguro.
- El nodo posee su script, y su referencia al nodo es un puntero normal: por eso no
  se puede formar un ciclo, y por eso `get_owner()` no necesita comprobar nada.
- Si escribes un `Node` nuevo y sobrescribes `update` o `draw`, **termina llamando a  `Node::update(...)` / `Node::draw(...)`**. Si no, el script de ese tipo deja de dispararse sin avisar.

## Señales

Un script avisa a otro script sin que ninguno guarde un puntero al otro. El emisor nombra
un evento; el oyente se conecta a ese nombre.

```cpp
class Enemigo : public Script<Node2D> {
public:
    void on_attached() override {
        declare_signal<int>("danio");        // opcional: emit_signal la declara sola
    }

    void recibirGolpe(int d) { emit_signal("danio", d); }
};

class BarraDeVida : public Script<NodeTextMatrix> {
public:
    explicit BarraDeVida(NodeRef<Node2D> enemigo) : enemigo(std::move(enemigo)) {}

    void on_attached() override {
        enemigo->connect_script_signal("danio", this, &BarraDeVida::al_recibir_danio);
    }

    void al_recibir_danio(int d) { golpes += d; }

    int golpes = 0;
    NodeRef<Node2D> enemigo;
};
```

La conexion se hace en `on_attached` porque ese es el hook de construccion: el subarbol
ya esta completo y el nodo ya tiene duena.

### Los reactors no cuelgan

Una conexion guarda **el id del nodo al que pertenece el reactor**. Antes de cada llamada
ese id se resuelve contra el `NodeRegistry`: si ya no resuelve, el reactor se descarta en
vez de llamarse.

Eso se apoya en un invariante que ya existia: `~Node` llama a `unregister_node()` antes de
destruir cualquier miembro, y todos los caminos de destruccion del motor pasan por ahi. Asi
que el id empieza a fallar exactamente cuando el dueno del reactor se vuelve inalcanzable,
que es justo cuando llamarlo seria comportamiento indefinido. No hay nada mas que registrar
ni que indexar: lo hace la tabla de ids del arbol.

### Quien es el dueno, y por que no es el emisor

El dueno es **el nodo al que pertenece el reactor**, no el nodo que tiene la senal. En el
caso habitual - un reactor de un script escuchando la senal de otro nodo - esa distincion
es la diferencia entre funcionar y un use-after-free:

```cpp
// El emisor suele vivir MAS que el oyente. Si la conexion fuese de el, este
// lambda seguiria llamandose con el script ya destruido.
enemigo->connect_signal<int>("danio", [this](int d){ ... });
```

Las formas de miembro leen el dueno del propio script, asi que esto ya no puede pasar por
alto:

```cpp
enemigo->connect_script_signal("danio", this, &BarraDeVida::al_recibir_danio);
```

Y cuando el dueno no es un script, el dueno es el nodo sobre el que conectaste. Para dar
uno explicitamente, el ultimo argumento de `Node::connect_signal`:

```cpp
emisor->connect_signal<int>("danio", [this](int d){ ... }, /*once=*/false, /*owner=*/dueno);
```

Consecuencias:

- Un script puede conectar sin guardar ids ni escribir desconexiones. La conexion se va
  con su nodo, sea cual sea el nodo que emita.
- Una conexion sin dueno (`connect` directo, o el `connect_signal` del bus sin nodo) **no**
  se descarta sola. Ese es el precio de no tener dueno, y se paga con
  `disconnect_signal(id)` o `SignalBus::clear()`.
- La limpieza es perezosa: ocurre en el siguiente `emit` de esa senal, no en el instante de
  la muerte. `is_connected(id)` sigue respondiendo `true` hasta que alguien emite.

### Firmas

El almacenamiento esta borrado - un nodo guarda `void(int)` y `void()` en la misma tabla -
pero la superficie publica no lo esta: `emit` y `connect` se plantillan sobre los tipos
reales. Por eso un reactor puede ser una funcion o una lambda corriente con parametros
corrientes, y un reactor cuyos parametros no encajan es un **error de compilacion**, no un
`tipo incorrecto` en ejecucion.

`signal<Args...>(nombre)` sobre un nombre declarado con otra firma lanza
`std::logic_error` diciendo como se declaro. Los nombres de tipo salen de `typeid`, o sea
que van mangled (`void(i)` en vez de `void(int)`).

Los argumentos de `emit_signal` se degradan, yeso es todo: se quitan las referencias y los
`const`, y **no** se convierten los enteros. `emit_signal("danio", d)` encuentra la senal
`void(int)` si `d` es `int`; si `d` es `unsigned` busca `void(unsigned)`, no encuentra nada y
lanza. Declara la senal para que case con lo que de verdad vas a pasar.

### Reactores, de mas simple a mas corto

```cpp
// 1. Lambda. Los argumentos hay que escribirlos; no se pueden leer de una lambda.
nodo->connect_signal<int>("danio", [this](int d){ vida -= d; });

// 2. Lambda sin argumentos, cuando la senal no lleva carga util.
nodo->connect_signal("murio", [this]{ mostrar_cadaver(); });

// 3. Miembro de un script. Los argumentos salen de su propia firma, sin lambda y sin
//    capturar `this`.
nodo->connect_script_signal("danio", this, &MiScript::al_recibir_danio);

// 4. Miembro de un script, sobre la senal de OTRO nodo. El dueno sigue siendo el
//    script, no `otro_nodo`. Dentro de un script, ScriptBase::connect_script_signal
//    hace lo mismo sin repetir el destino - y ademas lanza si ese destino no ha
//    declarado la senal, en vez de crear una que nadie va a emitir.
otro_nodo->connect_script_signal("danio", this, &MiScript::al_recibir_danio);

// 5. Sin dueno: hay que desconectar a mano. Para un reactor que no posee nada.
//    Ojo: la lambda tiene que poder llamarse con los argumentos de la senal.
nodo->connect_signal<int>("danio", [contador](int){ ++*contador; });
```

`connect_signal` toma el reactor como referencia reenviable y construye el `std::function`
adentro. No es un detalle: si el parametro fuera `std::function<void(Args...)>`, el
compilador intentaria deducir los argumentos de la propia lambda y no habria forma de
conectar una, ni siquiera escribiendo `<int>` en la llamada.

### Reentrada

`emit` recorre una **copia** de la lista de reactores, asi que un reactor puede conectar,
desconectar o liberar nodos mientras se ejecuta. Las reglas son fijas, no incidentales:

- Todos los reactores **persistentes** conectados cuando empezo el `emit` se ejecutan,
  aunque uno anterior los haya desconectado. No hay bandera de "cancelado", igual que en
  Godot.
- Un reactor conectado **durante** un `emit` no se ejecuta en ese `emit`.
- Un reactor `once` se borra **antes** de llamarse, asi que uno que lanza no queda armado.
  Y si un reactor anterior de la misma lista vuelve a emitir esta senal, el `once` no se
  dispara por segunda vez: ya no esta.

Los reactores con dueno muerto se limpian **al principio** del `emit`, no al final. No es
un detalle: un reactor puede llamar a `clear_signals()`, soltar el nodo que duena la senal
o borrar el `unique_ptr` que la contiene, y cualquiera de esas cosas destruye el objeto
sobre el que `emit` esta trabajando. Compacting antes significa que despues de llamar a un
reactor no queda nada que tocar.

Coste: un `std::function` por conexion, una copia de la lista por `emit` y una pasada de
compactado. Para un evento que se dispara unas pocas veces por frame es el intercambio
correcto; para una difusion por frame en un camino caliente, midelo.

### El bus global

Para lo que no pertenece a un nodo: la entrada, un cambio de escena, un estado de juego al
que cualquier parte de la escena pueda escuchar.

```cpp
SignalBus::get_instance().emit_signal("mouse_clicked", 1, Input::mouse_pos);
```

No es un sustituto de las senales de nodo. Si una senal tiene un duena claro, ese duena es un
nodo y la senal es suya, alcanzable por ruta. El bus es para lo que es ambiental de verdad, o
para cuando el emisor no sabe quien escucha.

Una unica instancia filtrada a proposito, igual que el `NodeRegistry`. El motor **no** la
limpia por ti al cambiar de escena: una conexion hecha para la escena siguiente puede ya
estar ahi. Llama a `clear()` donde signifique algo.

### Señales que ya emite el motor

`Button` declara `clicked` en su constructor y lo dispara en `update()`, en el frame en el que
aprieta el raton sobre el. Esa señal es la unica via que tiene un boton: antes tambien aceptaba
un `set_on_click`, y entonces decir "este boton hace algo" tenia dos respuestas distintas, de
las que solo una podia usarse a la vez. Un boton con dos reactores es un boton con una señal y
dos oyentes.

```cpp
// No hace falta guardar el handle al boton para oírlo.
salir->connect_script_signal("clicked", this, &MiScript::salir);
```

Y en el bus, desde `Input::refresh_input()`, que el bucle ya llama **antes** de
`process_scene()`, asi que un reactor ve el evento en el mismo frame:

| Señal | Carga | Cuando |
|---|---|---|
| `mouse_clicked` | `(int boton, Vec2 posicion)` | 1 = izquierdo, 2 = derecho. Como maximo una por frame, y si los dos botones caen en el mismo poll gana el izquierdo: el terminal entrega dos booleanos, no una cola de eventos |
| `mouse_moved` | `(Vec2 posicion)` | solo cuando la posicion **ha cambiado** desde el refresh anterior, y por tanto **tampoco en el primer frame**, cuando todavia no hay posicion anterior con la que comparar |

El boton es un `int` y no un enum a proposito, para que un reactor pueda compararlo sin
incluir esta cabecera.

**No hay `key_pressed` porque el motor no tiene entrada de teclado.** `IOSTerminal` expone
`poll_events`, el raton y lecturas de texto bloqueantes, y nada mas. Anadirla significa
meter un poll de teclas en `IOSTerminal` y en los dos backends primero; en ese momento
`key_pressed` encaja aqui al lado de las otras dos.

### Consultas y limpieza

```cpp
nodo->has_signal<int>("danio");     // false si no existe O si es de otra firma
nodo->get_signal_count();           // cuantas senales declara este nodo
nodo->is_connected(id);             // sigue vivo ese id de conexion
nodo->disconnect_signal(id);        // false si nadie lo tenia
nodo->clear_signals();              // suelta los reactores que ESCUCHAN a este nodo

SignalBus::get_instance().is_declared("mouse_clicked");   // ignora la firma
```

`is_declared` es la consulta que quiere un emisor del motor. `has_signal<Args...>` tambien
es `false` cuando el nombre existe con otra firma, y usar eso como guarda de un `emit`
convierte una unica suposicion equivocada en una senal que deja de dispararse en silencio
durante el resto del proceso: la propia guarda es la que lo hace silencioso. Con
`is_declared`, un reactor con la firma equivocada recibe el `std::logic_error` de
`emit_signal` en vez de nada.

Y ojo con `clear_signals()`: suelta los reactores que escuchan a **este** nodo. No toca las
conexiones que este nodo tenga en las senales de otros, que viven en la tabla de esos otros
nodos. Para un teardown de escena, `SignalBus::clear()` es lo que baja todo.
```

`has_signal` no es decorativo: es lo que hace que un emisor builtin cueste una busqueda en
un mapa en vez de una declaracion y una lista vacia de reactores. `clear_signals()` no lo
llama nadie del motor; existe para el caso de un teardown de escena, donde quieres soltarlos
antes de que nada mas empiece a emitir en la escena siguiente.

## Botones

`Button` es un `Control` con texto y un `StyleBox`, y reacciona al raton. Sustituye a
`NodeButton`, que vivia fuera del sistema de layout y por eso no se enteraba de contenedores ni
de anchors:

```cpp
NodeRef<Button> ok = panel->add_child<Button>("ok");
ok->set_text({"Ok"});
ok->get_stylebox(ButtonState::Normal).set_background_color("blue");
ok->get_stylebox(ButtonState::Hover).set_background_color("cyan");
ok->get_stylebox(ButtonState::Pressed).set_background_color("green");
ok->connect_script_signal("clicked", this, &MiScript::aceptar);
```

Cada estado lleva su propio `StyleBox` y su propio color de texto, asi que un hover puede ser
otro fondo y no un intercambio de fg/bg como hacia `NodeButton`. `draw` elige el que toca con
esta precedencia:

`Disabled` > `Pressed` > `Hover` > `Normal`

O sea que un boton desactivado no se enciende al pasar por encima ni aunque dejes el raton
apretado encima. Los tres estados que no son `Normal` arrancan vacios y `draw` cae al `Normal`
si el estado actual no tiene caja propia, asi que un boton sin configurar sigue dibujando.

**El tamano siempre es el del texto.** No hay bandera: un boton nunca puede acabar mas estrecho
que su propia etiqueta, porque eso es justo lo que no puede hacer un boton. Como en cualquier
`Control`, eso es un suelo y no un objetivo: `set_size()` y los anchors deciden el tamano real,
y un boton en una celda ancha se dibuja ancho. El minimo se mide desde `raw_text` solo, asi que
depende del contenido y no del sitio que le toque.

**Ojo con el primer frame.** El bucle llama a `update()` antes que a `update_layout()`, asi que
el primer frame no tiene rect resuelto y no puede acertar ningun `is_inside`. A partir del
segundo va bien. No es un problema del boton: es el orden del bucle.

No hay activacion por teclado ni foco, porque el motor no tiene entrada de teclado. Un boton se
alcanza solo con el raton.

## Contenedores

`Container` y sus tres derivados colocan a los hijos. Todos dan por hecho lo mismo, y conviene
saberlo antes de usarlos:

> Un contenedor es dueno de la posicion de sus hijos.

Un hijo dentro de un contenedor no conserva su `local_position`. El contenedor la reescribe en
cada paso de layout para centrarlo. Lo que sobrevive es la posicion **relativa** al bloque que
dio el contenedor: un hijo en `{0, 0}` queda en la esquina de ese bloque, uno en `{2, 1}` dos
columnas y una fila mas adentro. Cada paso quita el desplazamiento anterior antes de aplicar el
siguiente, asi que esa posicion relativa es estable entre frames en vez de acumularse.

```cpp
auto root = std::make_unique<Node>("root");

NodeRef<PanelContainer> panel = root->add_child<PanelContainer>("panel", Vec2{0, 0});
panel->set_anchor_preset(AnchorPreset::FullRect);
panel->get_stylebox().set_background_color("blue");
panel->get_stylebox().set_content_margin(1, 1, 1, 1);

NodeRef<Label> titulo = panel->add_child<Label>("titulo");
titulo->set_text({"Hola"});
titulo->set_anchor_preset(AnchorPreset::FullRect);
```

Los cuatro:

| Nodo | Que hace con sus hijos |
| --- | --- |
| `Container` | Los apila todos en el centro, encima unos de otros |
| `HBox` | Una celda por hijo, en fila; anchos se suman, la mas alta manda |
| `VBox` | Una celda por hijo, en columna; alturas se suman, la mas ancha manda |
| `PanelContainer` | Estira el hijo dentro del marco y dibuja ese marco con un `StyleBox` |

`HBox` y `VBox` reparten una celda por hijo a lo largo de su eje: la celda es exactamente el
minimo del hijo, y la separacion va entre celdas, nunca antes de la primera ni despues de la
ultima. La tira entera la centra despues el `Container` de base, igual que un `Container`
normal centra su bloque.

```cpp
NodeRef<HBox> fila = root->add_child<HBox>("fila", Vec2{0, 0});
fila->set_anchor_preset(AnchorPreset::FullRect);
fila->set_separation(2);

// Cada hijo reserva 10 columnas. El hijo sigue decidiendo como llena su celda:
// con FullRect se estira a todo el alto de la fila.
NodeRef<Label> izq = fila->add_child<Label>("izq");
izq->set_text({"izq"});
izq->set_custom_minimum_size({10, 1});
```

El hijo sigue siendo dueno de **como** llena su celda: el contenedor le da la celda y lo mueve
hasta ahi, y a partir de ahi mandan sus propios anchors y su `Grow`. Por eso un hijo que quiera
ocupar el alto entero de un `HBox` se pone `Grow::Both` o el preset `FullRect`.

`PanelContainer` es el unico que rellena en vez de centrar. Le da al hijo todo lo que queda
dentro del borde y de los margenes del `StyleBox` - el mismo que dibuja `Label`, con color de
fondo, borde y margen de contenido. El margen cuenta para el minimo del panel, porque es el
marco del panel y no relleno: un panel estrujado por debajo de sus margenes tiene que perder
contenido, no sus bordes. Se espera un solo hijo; con varios, todos se estirarian sobre la
misma zona y se pisarian.

Un detalle que no es obvio: solo los hijos `Control` aportan a `get_minimum_size()`. Un
`NodeTextMatrix` no tiene minimo que reportar, asi que no reserva sitio - pero si lo mueve el
centrado, porque es un hijo mas.

### `fit_content`: que un Label reserve sitio para su texto

Por defecto un `Label` no tiene minimo propio: se envuelve en el ancho que le da el layout y
recorta lo que no quepa. Eso es lo que quiere un Label metido en un marco de tamano fijo, pero
en un `HBox` significa que el texto no reserva nada y la celda sale a cero.

`fit_content` le dice que reporte el tamano que su texto pide:

```cpp
NodeRef<Label> titulo = fila->add_child<Label>("titulo");
titulo->set_text({"Hola"});
titulo->set_fit_content(true);   // ahora ocupa 4x1 y la fila lo cuenta
```

Con la propiedad activa, `get_minimum_size()` devuelve la linea mas larga del texto y el numero
de lineas que ocupa, mas el marco del `StyleBox` para que el texto quepa **dentro** del borde y
no debajo. Sin ella sigue mandando solo el minimo que haya puesto el llamante.

El minimo responde a una pregunta: **¿hasta donde puede encogerse esto sin que su texto quepa
dentro?** Esa respuesta depende solo del texto, no del sitio que le toque. Por eso se mide
partiendo `raw_text` por `\n` con un ancho infinito, y no mirando el wrap actual: un Label
estrechado reportaria las lineas cortas en las que lo apretaron, y uno estirado una sola linea
larga. Mismo texto, minimo distinto, decidido por el layout en vez de por el contenido.

Y ojo, que el minimo es un **suelo, no un tope**: `set_size()` y los anchors siguen mandando
sobre el tamano real, esto solo impide que se quede corto. Un `Label` con `fit_content` y
`FullRect` sigue ocupando toda la pantalla; la propiedad le dice a los contenedores cuanto
reservar, no le quita sitio a quien se lo da.

## Integracion CMake

El motor es una libreria estatica que no depende de nada externo. Tres modos de consumo:

**Como submódulo o copia local** (sin red, recomendado para uso offline):

```cmake
add_subdirectory(external/GuayabaTUIEngine)
target_link_libraries(mi_app PRIVATE GuayabaTUIEngine::GuayabaTUIEngine)
```

**Con FetchContent:**

```cmake
include(FetchContent)
FetchContent_Declare(GuayabaTUIEngine SOURCE_DIR external/GuayabaTUIEngine)
FetchContent_MakeAvailable(GuayabaTUIEngine)
target_link_libraries(mi_app PRIVATE GuayabaTUIEngine::GuayabaTUIEngine)
```

**Con find_package**, tras `cmake --install` del motor:

```cmake
find_package(GuayabaTUIEngine REQUIRED)
target_link_libraries(mi_app PRIVATE GuayabaTUIEngine::GuayabaTUIEngine)
```

El requisito de C++17 se propaga automaticamente: no hace falta `set(CMAKE_CXX_STANDARD 17)` en el proyecto consumidor.

## Estructura

```text
include/guayaba/     API publica. Ninguna cabecera de plataforma es alcanzable desde aqui.
  Guayaba.hpp        Header umbrella
  Core/              Vec2, Rect, TermChar, Color, Input, TextWrap, Script,
                     ScriptRef, Signal, SignalBus, NodeRef, NodeRegistry
  Terminal/          IOSTerminal (interfaz abstracta) + create_system_terminal()
  Renderer/          ConsoleRenderer
  Nodes/             Node, Node2D, NodeTextMatrix
    UI/              Control, Label, Button, Container,
                     HBox, VBox, PanelContainer
  Scene/             SceneManager
src/                 Implementaciones. Privado, no se instala.
  Core/              NodeRegistry, Signal, SignalBus,
                     DeletionQueue (cola de destruccion diferida)
  Terminal/          Backends Linux/macOS (termios) y Windows (Win32)
```

Los backends de plataforma viven exclusivamente en `src/Terminal/`, de modo que incluir
`<guayaba/Guayaba.hpp>` no filtra macros como `min`, `max` ni definidores de Win32 al
proyecto consumidor.

La destruccion diferida tambien vive en `src/Core/DeletionQueue.hpp`: es un detalle
interno, no API. Registra ids de nodo en vez de punteros, y al vaciar resuelve cada
uno contra el registro. Esa indireccion es la que hace que un `queue_free()` anidado
no destruya nada dos veces, y que un nodo que el consumidor ya reclamo con
`remove_child()` no se destruya en absoluto.

## Compilar el motor

```bash
cmake -S . -B build
cmake --build build
```

## IntelliSense en VSCode

Si al abrir `#include <guayaba/Guayaba.hpp>` aparece *Include errors detected*
(`C/C++(1696)`), **no es un fallo del codigo**: es que el editor no sabe donde esta
la carpeta `include/`. Los includes con `<>` necesitan que `include/` sea una raiz de
busqueda, algo que CMake configura solo y el editor no lee por su cuenta.

La solucion es la base de datos de compilacion, que CMake genera:

```bash
cmake -S . -B build
cmake --build build
```

Con eso, `cpptools` detecta `build/compile_commands.json` por su cuenta.

```bash
ln -sf build/compile_commands.json compile_commands.json
```

Nota para Windows: `compile_commands.json` solo se genera con los generadores
Makefile o Ninja. El generador Visual Studio (el default de `cmake-gui.exe`) no lo
produce; en ese caso hay que configurar con `-G Ninja` o anadir a mano
`.vscode/c_cpp_properties.json` con `includePath` apuntando a `include/`.
