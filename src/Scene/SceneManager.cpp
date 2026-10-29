#include <guayaba/Core/Focus.hpp>
#include <guayaba/Core/Input.hpp>
#include <guayaba/Core/Rect.hpp>
#include <guayaba/Core/SignalBus.hpp>
#include <guayaba/Nodes/Node.hpp>
#include <guayaba/Nodes/UI/Control.hpp>
#include <guayaba/Renderer/ConsoleRenderer.hpp>
#include <guayaba/Scene/SceneManager.hpp>
#include <guayaba/Terminal/IOSTerminal.hpp>

#include <chrono>
#include <thread>

#include "../Core/DeletionQueue.hpp"
#include "../Core/MouseCapture.hpp"

namespace guayaba {

SceneManager& SceneManager::get_instance(){
    static SceneManager instance;
    return instance;
}

void SceneManager::process_scene() {
    auto current_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = current_time - m_lastTime;
    m_deltaTime = elapsed.count();
    m_lastTime = current_time;

    if (!root_node) return;
    root_node->update(m_deltaTime);
}

NodeRef<Node> SceneManager::get_current_scene() {
    return NodeRef<Node>(root_node.get());
}

void SceneManager::change_scene(std::unique_ptr<Node> new_root) {
    next_scene = std::move(new_root);
}

std::unique_ptr<Node> SceneManager::take_scene() {
    return std::move(root_node);
}

void SceneManager::apply_pending_scene() {
    if (!next_scene) return;

    // Before exit_tree, so a control still exists to hear focus_exited. The
    // capture does not survive a scene change either.
    FocusManager::get_instance().clear();
    detail::MouseCapture::instance().clear();
    if (root_node) root_node->exit_tree();
    root_node = std::move(next_scene);
    if (root_node) root_node->enter_tree();
}

void SceneManager::run_frame(ConsoleRenderer& renderer, std::vector<InputEvent> events) {
    apply_pending_scene();

    if (!root_node) return;

    renderer.clear_buffer();

    /*
        Five steps, in this order, and the order is the contract:

          update_layout()  where everything is, knowing that
          route_input()    who acts on it, against those rects
          process_scene()  per-frame logic, seeing the result
          draw()           the picture

        Routing hit-tests, so layout must precede it: a control that resized this
        frame is tested against the size it will be drawn at. Input precedes
        update, so a control that acted on a key has done so before a script reads
        its state in the same frame.
    */

    // After the caller's resize, so Controls measure against the buffer size
    // about to be painted.
    this->root_node->update_layout(Rect::from_size(renderer.get_buffer_size()));

    // The captor sees mouse input before the tree, wherever the pointer is; what
    // it declines continues through the normal walk.
    Control::route_captured_input(events);

    this->root_node->route_input(events);

    // Whatever nobody consumed, offered to scripts and to the bus as the
    // unhandled fallback.
    if (!events.empty()) {
        SignalBus& bus = SignalBus::get_instance();
        const bool bus_listens = bus.is_declared("unhandled_input");
        for (const InputEvent& event : events) {
            this->root_node->route_unhandled_input(event);
            if (bus_listens) bus.emit_signal("unhandled_input", event);
        }
    }

    this->process_scene();

    // The clip starts as the whole buffer, and narrows from there as it passes
    // down through any Control that clips.
    this->root_node->draw(renderer, Rect::from_size(renderer.get_buffer_size()));

    // Last, so a node queued by on_process still draws its final frame and is
    // gone before the next frame starts.
    detail::drain_pending_deletions();

    renderer.present();
    increase_frame_count();
}

void SceneManager::start_running() {
    this->is_running = true;

    // Create the platform-correct terminal
    os_terminal = create_system_terminal();
    os_terminal->init();

    // Wire up the global Input system
    Input::g_terminal = os_terminal.get();

    m_lastTime = std::chrono::high_resolution_clock::now();

    Vec2 buffer_size = {100, 60};
    ConsoleRenderer renderer(buffer_size, os_terminal.get());

    // Restores the terminal if the loop below throws on its way out, which a
    // statement at the end of the function would not. The exception still
    // propagates.
    struct TerminalRestoreGuard {
        IOSTerminal* terminal;
        ~TerminalRestoreGuard() { if (terminal) terminal->restore(); }
    } terminal_guard{os_terminal.get()};

    while (this->is_running) {

        // Taken once per frame, from the same clock the budget below uses.
        const auto frame_start = std::chrono::steady_clock::now();

        // Applied before polling input, so a scene that entered this frame is
        // already listening to the bus signals refresh_input() emits.
        apply_pending_scene();

        if (this->is_running && this->root_node) {
            renderer.resize(os_terminal->get_screen_size(renderer.get_buffer_size()));
            Input::refresh_input();
        }

        // The rest of the frame lives here: clear, layout, routing, update, draw
        // and present. The events are taken once and handed over by value, so a
        // control can erase the ones it consumed.
        run_frame(renderer, Input::drain_input_events());

        /*
            Spend whatever is left of the frame's budget waiting. Placed after
            present(), so the terminal has just been written to and now has time
            to be read. A frame that overran its budget gets none of this, so one
            slow frame does not push every later one further behind.
        */
        if (target_fps > 0 && this->is_running && this->root_node) {
            using clock = std::chrono::steady_clock;
            const auto spent = std::chrono::duration_cast<std::chrono::microseconds>(
                clock::now() - frame_start);
            const auto budget = std::chrono::microseconds(1000000 / target_fps);
            if (spent < budget) std::this_thread::sleep_for(budget - spent);
        }
    }

    // Teardown: dropping the root can itself queue deletions from destructors,
    // so drain once more afterwards.
    if (root_node) {
        root_node->exit_tree();
        root_node.reset();
    }
    detail::drain_pending_deletions();

    // Terminal restoration is terminal_guard's job; see its definition above.
}

void SceneManager::stop_running() {
    this->is_running = false;
}

void SceneManager::set_target_fps(int fps) {
    this->target_fps = fps > 0 ? fps : 0;
}

int SceneManager::get_target_fps() const {
    return this->target_fps;
}

unsigned long int SceneManager::get_frame_count() const{
    return frame_count;
}

void SceneManager::increase_frame_count(){
    frame_count++;
}

} // namespace guayaba