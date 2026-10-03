#include <guayaba/Core/Input.hpp>
#include <guayaba/Nodes/Node.hpp>
#include <guayaba/Renderer/ConsoleRenderer.hpp>
#include <guayaba/Scene/SceneManager.hpp>
#include <guayaba/Terminal/IOSTerminal.hpp>

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

std::shared_ptr<Node> SceneManager::get_current_scene() {
    return root_node;
}

void SceneManager::change_scene(std::shared_ptr<Node> new_root) {
    if (new_root == root_node) return;
    next_scene = new_root;
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

        if (next_scene){
            if (root_node) {
                root_node->exit_tree();
            }
            root_node = next_scene;
            next_scene = nullptr;
            if (root_node) {
                root_node->enter_tree();
            }
        }

        if (this->is_running && this->root_node) {

            renderer.resize(os_terminal->get_screen_size(renderer.get_buffer_size()));
            renderer.clear_buffer();
            Input::refresh_input();

            this->process_scene();
            this->root_node->draw(renderer);
            renderer.present();
            increase_frame_count();
        }
    }

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