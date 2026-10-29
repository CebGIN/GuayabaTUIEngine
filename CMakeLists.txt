cmake_minimum_required(VERSION 3.14)

project(GuayabaTUIEngine
    VERSION 0.1.0
    DESCRIPTION "Motor de interfaces de usuario para terminal, multiplataforma, con soporte de raton"
    LANGUAGES CXX
)

if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Sanitizers (opt-in) ------------------------------------------------------------
# Instruments the library and the tests. GCC/Clang only: MSVC has no equivalent
# spelling, and silently ignoring the option would hide that.
option(GUAYABA_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if(GUAYABA_SANITIZE)
    if(MSVC)
        message(FATAL_ERROR "GUAYABA_SANITIZE is only supported with GCC and Clang.")
    endif()
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# Compilation database for IntelliSense (cpptools, CLion, clangd, vim).
# Only when this project is the top-level one: the variable applies to the whole
# build tree, so enabling it unconditionally would hand a compile_commands.json
# to any project that includes us via add_subdirectory().
# Note: requires the Makefile or Ninja generator. The Visual Studio generator
# on Windows does not produce one.
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

# --- Seleccion del backend de plataforma -----------------------------------------
# Solo se compila el backend del sistema actual. Los .cpp descartados tambien
# estan protegidos con #ifdef, asi que la exclusion aqui es solo de higiene.
if(WIN32)
elseif(UNIX)
else()
    message(FATAL_ERROR
        "GuayabaTUIEngine: plataforma no soportada. Solo Windows, Linux y macOS.")
endif()

# --- Fuentes ----------------------------------------------------------------------
file(GLOB_RECURSE GUAYABA_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

if(WIN32)
    list(REMOVE_ITEM GUAYABA_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/Terminal/LinuxTerminal.cpp")
else()
    list(REMOVE_ITEM GUAYABA_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/Terminal/WindowsTerminal.cpp")
endif()

# --- Libreria estatica -------------------------------------------------------------
add_library(GuayabaTUIEngine STATIC ${GUAYABA_SOURCES})
add_library(GuayabaTUIEngine::GuayabaTUIEngine ALIAS GuayabaTUIEngine)

# cxx_std_17 como requisito PUBLIC: se propaga a los proyectos consumidores.
target_compile_features(GuayabaTUIEngine PUBLIC cxx_std_17)

set_target_properties(GuayabaTUIEngine PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_EXTENSIONS OFF
)

include(GNUInstallDirs)

# $<BUILD_INTERFACE> cubre add_subdirectory()/FetchContent sin instalar;
# $<INSTALL_INTERFACE> cubre find_package().
target_include_directories(GuayabaTUIEngine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# --- Instalacion / exportacion -------------------------------------------------------
install(TARGETS GuayabaTUIEngine
    EXPORT GuayabaTUIEngineTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT GuayabaTUIEngineTargets
    FILE GuayabaTUIEngineTargets.cmake
    NAMESPACE GuayabaTUIEngine::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GuayabaTUIEngine
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/GuayabaTUIEngineConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GuayabaTUIEngineConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/GuayabaTUIEngineConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GuayabaTUIEngine
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/GuayabaTUIEngineConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/GuayabaTUIEngineConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GuayabaTUIEngine
)

# --- Tests ---------------------------------------------------------------------
# Off by default so a project that consumes the engine through
# add_subdirectory() inherits no extra targets. Enabled on the top level with
# -DGUAYABA_BUILD_TESTS=ON, the tests lay out scenes in memory without a
# terminal - see tests/layout_alignment.cpp.
option(GUAYABA_BUILD_TESTS "Build GuayabaTUIEngine's headless layout tests" OFF)

if(GUAYABA_BUILD_TESTS)
    # Warnings only on the tests: the library stays as it was, and nothing here
    # is -Werror, so a stricter compiler does not break the build.
    if(MSVC)
        add_compile_options(/W4)
    else()
        add_compile_options(-Wall -Wextra)
    endif()

    add_executable(layout_alignment tests/layout_alignment.cpp)
    target_link_libraries(layout_alignment PRIVATE GuayabaTUIEngine)

    add_executable(key_parsing tests/key_parsing.cpp)
    target_link_libraries(key_parsing PRIVATE GuayabaTUIEngine)

    add_executable(input_routing tests/input_routing.cpp)
    target_link_libraries(input_routing PRIVATE GuayabaTUIEngine)

    add_executable(line_edit tests/line_edit.cpp)
    target_link_libraries(line_edit PRIVATE GuayabaTUIEngine)

    add_executable(graph tests/graph.cpp)
    target_link_libraries(graph PRIVATE GuayabaTUIEngine)

    add_executable(listview tests/listview.cpp)
    target_link_libraries(listview PRIVATE GuayabaTUIEngine)

    add_executable(text_rendering tests/text_rendering.cpp)
    target_link_libraries(text_rendering PRIVATE GuayabaTUIEngine)

    add_executable(frame_order tests/frame_order.cpp)
    target_link_libraries(frame_order PRIVATE GuayabaTUIEngine)
endif()
