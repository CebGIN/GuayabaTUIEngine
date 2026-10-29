#include <guayaba/Core/Focus.hpp>
#include <guayaba/Core/Input.hpp>
#include <guayaba/Core/Rect.hpp>
#include <guayaba/Core/SignalBus.hpp>
#include <guayaba/Nodes/Node.hpp>
#include <guayaba/Renderer/ConsoleRenderer.hpp>
#include <guayaba/Scene/SceneManager.hpp>
#include <guayaba/Terminal/IOSTerminal.hpp>

#include <chrono>
#include <thread>

#include "../Core/DeletionQueue.hpp"

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

        // Scene change.
        if (next_scene){
            // Before exit_tree, so a control still exists to hear focus_exited.
            FocusManager::get_instance().clear();
            if (root_node) root_node->exit_tree();
            root_node = std::move(next_scene);
            if (root_node) root_node->enter_tree();
        }

        if (this->is_running && this->root_node) {

            renderer.resize(os_terminal->get_screen_size(renderer.get_buffer_size()));
            renderer.clear_buffer();

            /*
                Five steps, in this order, and the order is the contract:

                  refresh_input()  what the user did this frame
                  update_layout()  where everything is, knowing that
                  route_input()    who acts on it, against those rects
                  process_scene()  per-frame logic, seeing the result
                  draw()           the picture

                Routing hit-tests, so layout must precede it: a control that
                resized this frame is tested against the size it will be drawn
                at. Input precedes update, so a control that acted on a key has
                done so before a script reads its state in the same frame.
            */
            Input::refresh_input();

            // Held in a local: route_input takes the vector by reference so a
            // control can erase a consumed event.
            std::vector<InputEvent> events = Input::drain_input_events();

            // After the resize above, so Controls measure against the buffer size
            // about to be painted.
            this->root_node->update_layout(Rect::from_size(renderer.get_buffer_size()));

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

            // The clip starts as the whole buffer, and narrows from there as
            // it passes down through any Control that clips.
            this->root_node->draw(renderer, Rect::from_size(renderer.get_buffer_size()));

            // Last, so a node queued by on_process still draws its final frame
            // and is gone before the next frame starts.
            detail::drain_pending_deletions();

            renderer.present();
            increase_frame_count();
        }

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