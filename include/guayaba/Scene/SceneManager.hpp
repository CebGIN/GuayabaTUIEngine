#ifndef GUAYABA_SCENE_SCENEMANAGER_HPP
#define GUAYABA_SCENE_SCENEMANAGER_HPP

#include <chrono>
#include <memory>

#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Terminal/IOSTerminal.hpp>

namespace guayaba {

class Node;

class SceneManager {
    private:
        std::shared_ptr<Node> root_node = nullptr;

        bool is_running = false;
        SceneManager() = default;
        SceneManager(const SceneManager&) = delete;
        SceneManager& operator=(const SceneManager&) = delete;

        std::chrono::high_resolution_clock::time_point m_lastTime;
        double m_deltaTime = 0.0;

        unsigned long int frame_count = 0;

        std::shared_ptr<Node> next_scene = nullptr;

        std::unique_ptr<IOSTerminal> os_terminal;

    public:
        static SceneManager& get_instance();

        void process_scene();

        std::shared_ptr<Node> get_current_scene();

        void change_scene(std::shared_ptr<Node> new_root);

        void start_running();

        void stop_running();

        unsigned long int get_frame_count() const;

        void increase_frame_count();
};

} // namespace guayaba

#endif // GUAYABA_SCENE_SCENEMANAGER_HPP