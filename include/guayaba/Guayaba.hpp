#ifndef GUAYABA_GUAYABA_HPP
#define GUAYABA_GUAYABA_HPP

/*
    Master umbrella header. Including this pulls in the entire public API:

        #include <guayaba/Guayaba.hpp>
        using namespace guayaba;

    No platform header (<windows.h>, <termios.h>) is reachable from here; the
    backends live entirely inside src/Terminal/.
*/

#if defined(_WIN32)
#elif defined(__linux__) || defined(__APPLE__)
#else
    #error "Unsupported platform. GuayabaTUIEngine supports Windows and Linux."
#endif

#include <guayaba/Core/Color.hpp>
#include <guayaba/Core/Input.hpp>
#include <guayaba/Core/NodeType.hpp>
#include <guayaba/Core/TermChar.hpp>
#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/Node.hpp>
#include <guayaba/Nodes/Node2D.hpp>
#include <guayaba/Nodes/NodeBox.hpp>
#include <guayaba/Nodes/NodeButton.hpp>
#include <guayaba/Nodes/NodeSQ.hpp>
#include <guayaba/Nodes/NodeUI.hpp>
#include <guayaba/Renderer/ConsoleRenderer.hpp>
#include <guayaba/Scene/SceneManager.hpp>
#include <guayaba/Terminal/IOSTerminal.hpp>
#include <guayaba/UI/UIPrefabs.hpp>

#endif // GUAYABA_GUAYABA_HPP