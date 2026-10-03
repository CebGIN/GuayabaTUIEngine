# GuayabaTUIEngine

Lightweight pure C++17 engine for building console user interfaces, with mouse interaction, cross-compatible between Windows and Linux.

## Uso

```cpp
#include <guayaba/Guayaba.hpp>
using namespace guayaba;

std::shared_ptr<Node> crearEscena() {
    auto root  = std::make_shared<Node>("root");
    auto box   = std::make_shared<NodeBox>("box", Vec2{0, 0}, Vec2{20, 6}, Color::BLUE);
    auto label = std::make_shared<NodeUI>("label", Vec2{2, 2}, Color::BLACK, Color::WHITE,
                                          std::vector<std::string>{"Hola"});
    box->add_child(label);
    root->add_child(box);
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
- Las funciones de la forma `std::shared_ptr<Node> crearEscena(){ ... return root; }`.
- En el `main()`: crear la root, establecerla como escena activa con `change_scene()`, y arrancar el bucle con `start_running()`.

## Prefabs

`guayaba::pui` incluye `confirm_dialog`, `h_slider` y `v_slider`.

```cpp
auto dialogo = pui::confirm_dialog("Confirmar", []{ /* ... */ });
```

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
  Core/              Vec2, TermChar, Color, Input, NodeType
  Terminal/          IOSTerminal (interfaz abstracta) + create_system_terminal()
  Renderer/          ConsoleRenderer
  Nodes/             Node, Node2D, NodeUI, NodeSQ, NodeBox, NodeButton
  Scene/             SceneManager
  UI/                UIPrefabs (namespace guayaba::pui)
src/                 Implementaciones. Privado, no se instala.
  Terminal/          Backends Linux/macOS (termios) y Windows (Win32)
examples/consumer/   Demo interactiva + test de API con backend simulado
```

Los backends de plataforma viven exclusivamente en `src/Terminal/`, de modo que incluir
`<guayaba/Guayaba.hpp>` no filtra macros como `min`, `max` ni definidores de Win32 al
proyecto consumidor.

## Compilar los ejemplos

```bash
cmake -S examples/consumer -B build -DGUAYABA_CONSUMER_MODE=add_subdirectory
cmake --build build
./build/api_test      # test de API, sin terminal
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
