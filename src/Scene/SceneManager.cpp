#include <guayaba/Core/Input.hpp>
#include <guayaba/Nodes/Node.hpp>
#include <guayaba/Renderer/ConsoleRenderer.hpp>
#include <guayaba/Scene/SceneManager.hpp>
#include <guayaba/Terminal/IOSTerminal.hpp>

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

    while (this->is_running) {
        
        //Cambio de escena
        if (next_scene){
            if (root_node) root_node->exit_tree();
            root_node = std::move(next_scene);
            if (root_node) root_node->enter_tree();
        }

        if (this->is_running && this->root_node) {

            renderer.resize(os_terminal->get_screen_size(renderer.get_buffer_size()));
            renderer.clear_buffer();
            Input::refresh_input();

            this->process_scene();
            this->root_node->draw(renderer);

            // Last, so that anything freed earlier in the frame is already gone
            // before the next one starts, and so a node queued by on_process still
            // draws its final frame.
            detail::drain_pending_deletions();

            renderer.present();
            increase_frame_count();
        }
    }

    // Teardown: dropping the root can itself queue deletions from destructors,
    // so drain once more afterwards.
    if (root_node) {
        root_node->exit_tree();
        root_node.reset();
    }
    detail::drain_pending_deletions();

    // Restore terminal on exit
    if (os_terminal) os_terminal->restore();
}

void SceneManager::stop_running() {
    this->is_running = false;
}

unsigned long int SceneManager::get_frame_count() const{
    return frame_count;
}

void SceneManager::increase_frame_count(){
    frame_count++;
}

} // namespace guayaba