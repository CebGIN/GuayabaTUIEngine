# GuayabaTUIEngine

Lightweight pure C++17 engine for building console user interfaces, with mouse interaction, cross-compatible between Windows and Linux.

## Uso

```cpp
#include <guayaba/Guayaba.hpp>
using namespace guayaba;

std::unique_ptr<Node> crearEscena() {
    auto root = std::make_unique<Node>("root");

    NodeBox* box = root->add_child<NodeBox>("box", Vec2{0, 0}, Vec2{20, 6}, Color::BLUE);
    box->add_child<NodeUI>("label", Vec2{2, 2}, Color::BLACK, Color::WHITE,
                          std::vector<std::string>{"Hola"});

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
NodeRef<NodeUI> titulo = root->add_child<NodeUI>(...); // el arbol ya lo duene
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
root->get_node<NodeUI>("fondo/titulo");       // relativa
root->get_node<NodeUI>("/fondo/titulo");      // absoluta, incluye la raiz
root->get_node_or_null<NodeUI>("quizas");     // vacio si no existe
node->get_node<NodeUI>("../label");           // "." y ".." tambien
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

Solo ensanchan: un `NodeRef<NodeUI>` se convierte implicitamente en
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
| `on_draw(renderer)` | tras el dibujado del nodo y **antes** que el de sus hijos |

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

`on_draw` se dibuja antes que los hijos a proposito, para que el `on_draw` de un hijo
siga siendo un overlay de verdad por encima del padre.

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
  Core/              Vec2, TermChar, Color, Input, NodeType, Script,
                     ScriptRef, NodeRef, NodeRegistry
  Terminal/          IOSTerminal (interfaz abstracta) + create_system_terminal()
  Renderer/          ConsoleRenderer
  Nodes/             Node, Node2D, NodeUI, NodeSQ, NodeBox, NodeButton
  Scene/             SceneManager
src/                 Implementaciones. Privado, no se instala.
  Core/              NodeRegistry, DeletionQueue (cola de destruccion diferida)
  Terminal/          Backends Linux/macOS (termios) y Windows (Win32)
examples/consumer/   Demo interactiva con backend simulado
```

Los backends de plataforma viven exclusivamente en `src/Terminal/`, de modo que incluir
`<guayaba/Guayaba.hpp>` no filtra macros como `min`, `max` ni definidores de Win32 al
proyecto consumidor.

La destruccion diferida tambien vive en `src/Core/DeletionQueue.hpp`: es un detalle
interno, no API. Registra ids de nodo en vez de punteros, y al vaciar resuelve cada
uno contra el registro. Esa indireccion es la que hace que un `queue_free()` anidado
no destruya nada dos veces, y que un nodo que el consumidor ya reclamo con
`remove_child()` no se destruya en absoluto.

## Compilar los ejemplos

```bash
cmake -S examples/consumer -B build -DGUAYABA_CONSUMER_MODE=add_subdirectory
cmake --build build

./build/consumer      # demo interactiva
```

`GUAYABA_CONSUMER_MODE` acepta `add_subdirectory`, `FetchContent` o `find_package`
(este ultimo necesita `CMAKE_PREFIX_PATH` apuntando al motor instalado).

## IntelliSense en VSCode

Si al abrir `#include <guayaba/Guayaba.hpp>` aparece *Include errors detected*
(`C/C++(1696)`), **no es un fallo del codigo**: es que el editor no sabe donde esta
la carpeta `include/`. Los includes con `<>` necesitan que `include/` sea una raiz de
busqueda, algo que CMake configura solo y el editor no lee por su cuenta.

La solucion es la base de datos de compilacion, que CMake genera:

```bash
cmake -S . -B build
cmake --build build
cmake -S examples/consumer -B build/consumer -DGUAYABA_CONSUMER_MODE=add_subdirectory
cmake --build build/consumer
```

Con eso, `cpptools` detecta `build/compile_commands.json` y `build/consumer/compile_commands.json`
por su cuenta. Si tu version no los busca en subdirectorios, crea un enlace:

```bash
ln -sf build/compile_commands.json compile_commands.json
```

Nota para Windows: `compile_commands.json` solo se genera con los generadores
Makefile o Ninja. El generador Visual Studio (el default de `cmake-gui.exe`) no lo
produce; en ese caso hay que configurar con `-G Ninja` o anadir a mano
`.vscode/c_cpp_properties.json` con `includePath` apuntando a `include/`.
