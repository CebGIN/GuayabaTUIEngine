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
#include <guayaba/Core/Focus.hpp>
#include <guayaba/Core/Input.hpp>
#include <guayaba/Core/InputEvent.hpp>
#include <guayaba/Core/Key.hpp>
#include <guayaba/Core/NodeRef.hpp>
#include <guayaba/Core/NodeRegistry.hpp>
#include <guayaba/Core/Script.hpp>
#include <guayaba/Core/ScriptRef.hpp>
#include <guayaba/Core/Signal.hpp>
#include <guayaba/Core/SignalBus.hpp>
#include <guayaba/Core/TermChar.hpp>
#include <guayaba/Core/Rect.hpp>
#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/Node.hpp>
#include <guayaba/Nodes/Node2D.hpp>
#include <guayaba/Nodes/NodeTextMatrix.hpp>
#include <guayaba/Nodes/UI/Button.hpp>
#include <guayaba/Nodes/UI/Control.hpp>
#include <guayaba/Nodes/UI/Container.hpp>
#include <guayaba/Nodes/UI/Graph.hpp>
#include <guayaba/Nodes/UI/GridContainer.hpp>
#include <guayaba/Nodes/UI/HBox.hpp>
#include <guayaba/Nodes/UI/Label.hpp>
#include <guayaba/Nodes/UI/LineEdit.hpp>
#include <guayaba/Nodes/UI/ListView.hpp>
#include <guayaba/Nodes/UI/MarginContainer.hpp>
#include <guayaba/Nodes/UI/PanelContainer.hpp>
#include <guayaba/Nodes/UI/ProgressBar.hpp>
#include <guayaba/Nodes/UI/ScrollView.hpp>
#include <guayaba/Nodes/UI/VBox.hpp>
#include <guayaba/Renderer/ConsoleRenderer.hpp>
#include <guayaba/Scene/SceneManager.hpp>
#include <guayaba/Terminal/IOSTerminal.hpp>

#endif // GUAYABA_GUAYABA_HPP