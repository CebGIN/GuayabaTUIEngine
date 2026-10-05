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
auto root = std::make_unique<Node>("root");  // tu lo duenes
NodeUI* titulo = root->add_child<NodeUI>(...); // el arbol ya lo duene
SceneManager::get_instance().change_scene(std::move(root));
```

Lo que se devuelve de `add_child<T>(...)` es un puntero crudo que **no** es dueno.
Es seguro mientras el nodo siga dentro del subarbol que te contiene, porque los
ancestros y los hermanos de tu propio subarbol no pueden liberarse antes que tu.
Para referencias que cruzan ese limite usa `NodeRef<T>`.

### Tabla de operaciones

| Operacion | Quien queda como dueno |
|---|---|
| `add_child<T>(...)` / `add_child(std::move(p))` | el arbol |
| `remove_child(n)` | tu, mediante el `unique_ptr` que devuelve |
| `change_scene(std::move(root))` | el `SceneManager` |
| `take_scene()` | tu, otra vez |
| un nodo que nunca se inserto | tu |

`remove_child()` devuelve `std::unique_ptr<Node>`, no destruye nada. Si no te
quedas con el `unique_ptr` que devuelve, el subarbol se va con el. No existe la
fuga de "quite el nodo del arbol y me olvide de liberarlo" que si admite un
`remove_child` que devuelve `void`.

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

Las rutas se resuelven contra el arbol, no se guardan, asi que sobreviven a un
`reparent`:

```cpp
root->get_node<NodeUI>("fondo/titulo");       // relativa
root->get_node<NodeUI>("/fondo/titulo");      // absoluta, incluye la raiz
root->get_node_or_null<NodeUI>("quizas");     // nullptr si no existe
node->get_node<NodeUI>("../label");           // "." y ".." tambien
```

`get_node()` lanza excepcion si no encuentra el nodo; `get_node_or_null()` devuelve
vacio. La primera segmento de una ruta absoluta nombra a la raiz, de modo que
`get_node(get_path())` siempre funciona.

Los nombres de hermanos se hacen unicos: si anades dos hijos llamados `box`, el
segundo queda como `box@2`. Sin eso las rutas serian ambiguas.

Para referencias que cruzan subarboles, `NodeRef<T>`:

```cpp
class Follower : public Script {
    NodeRef<Node2D> target;   // vive en el subarbol de otro
    void on_process(double) override {
        if (auto t = target.get()) t->set_local_position(Input::mouse_pos);
    }
};
```

Son 8 bytes, no poseen nada, y `get()` devuelve `nullptr` en cuanto el nodo se
destruye, sin importar quantas veces se reutilice esa direccion. No detectan una
referencia que sigue siendo valida pero quedo obsoleta (por ejemplo, tras un
`reparent`): para eso esta `get_node_or_null()`.

## Logica propia: Script

Un `Script` cuelga logica y estado de un `Node` existente **sin derivar de la clase
C++** de ese nodo. La idea es el `Script` de Godot: poder añadir comportamiento a
un prefab que no escribiste, sin reimplementar su dibujado.

```cpp
class Parpadeo : public guayaba::Script {
public:
    float periodo = 0.5f;
    float transcurrido = 0.0f;

    void on_process(double dt) override {
        transcurrido += dt;
        if (transcurrido >= periodo) { transcurrido = 0.0f; /* invierte */ }
    }
};

auto* parpadeo = nodo->add_script<Parpadeo>();
parpadeo->periodo = 0.2f;   // accede a tu campo por tu propia referencia
```

**Guarda la referencia que ya tienes.** El `Parpadeo*` que devuelve
`add_script<T>()` es la via corta: sin casts y sin busquedas. `get_script_as<T>()`
existe para cuando solo tienes el nodo. Las propiedades son campos tipados de tu
subclase, sin mapa ni casts.

`get_owner()` te da un `Node*`, y `get_owner_as<T>()` el tipo concreto. No hay ni
un bloqueo ni un contador de referencias: el nodo es dueno de su script, asi que
mientras corre cualquiera de tus metodos el nodo esta vivo por construccion.

### Hooks disponibles

| Hook | Cuando salta |
|---|---|
| `on_attached` / `on_detached` | al ligar o desligar el script de un nodo |
| `on_enter_tree` / `on_exit_tree` | solo si el nodo entra o sale del arbol de escena |
| `on_process(dt)` | cada frame, con el nodo en el arbol |
| `on_draw(renderer)` | tras el dibujado del nodo y **antes** que el de sus hijos |

Los dos pares son ciclos distintos y no se mezclan. En particular, si adjuntas un
script a un nodo que ya esta vivo, `on_enter_tree` no llegara solo: pidelo tu.

```cpp
void on_attached() override {
    if (get_owner() && get_owner()->is_in_tree()) on_enter_tree();
}
```

`on_draw` se dibuja antes que los hijos a proposito, para que el `on_draw` de un hijo
siga siendo un overlay de verdad por encima del padre.

### Notas

- Como maximo un script por nodo. Adjuntar `nullptr`, o un script ya ligado a otro
  nodo, es un no-op silencioso; sustituir el script del nodo dispara `on_detached` en el anterior.
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
                     NodeRef, NodeRegistry
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
