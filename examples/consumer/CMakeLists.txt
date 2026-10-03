cmake_minimum_required(VERSION 3.14)

project(GuayabaConsumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compilation database for IntelliSense. Same guard as in the engine: only when
# this project is the top-level one.
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

# Modo de consumo del motor. Los tres deben producir el mismo binario.
#   add_subdirectory  -> cmake -DGUAYABA_CONSUMER_MODE=add_subdirectory
#   FetchContent      -> cmake -DGUAYABA_CONSUMER_MODE=FetchContent
#   find_package      -> cmake -DGUAYABA_CONSUMER_MODE=find_package
#                        (tras `cmake --install` del motor)
set(GUAYABA_CONSUMER_MODE "add_subdirectory" CACHE STRING "add_subdirectory|FetchContent|find_package")

set(GUAYABA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")

if(GUAYABA_CONSUMER_MODE STREQUAL "add_subdirectory")
    # Uso como submódulo o copia local: compila offline, sin instalar nada.
    add_subdirectory("${GUAYABA_ROOT}" guayaba_build)
elseif(GUAYABA_CONSUMER_MODE STREQUAL "FetchContent")
    include(FetchContent)
    FetchContent_Declare(GuayabaTUIEngine SOURCE_DIR "${GUAYABA_ROOT}")
    FetchContent_MakeAvailable(GuayabaTUIEngine)
elseif(GUAYABA_CONSUMER_MODE STREQUAL "find_package")
    find_package(GuayabaTUIEngine REQUIRED)
else()
    message(FATAL_ERROR "GUAYABA_CONSUMER_MODE desconocido: ${GUAYABA_CONSUMER_MODE}")
endif()

# Demo interactiva: arranca el bucle de eventos real.
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE GuayabaTUIEngine::GuayabaTUIEngine)

# Test de API sin terminal: comprueba los 3 criterios de aceptacion.
add_executable(api_test api_test.cpp)
target_link_libraries(api_test PRIVATE GuayabaTUIEngine::GuayabaTUIEngine)

enable_testing()
add_test(NAME api_test COMMAND api_test)