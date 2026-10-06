#ifndef GUAYABA_SCENE_SCENEMANAGER_HPP
#define GUAYABA_SCENE_SCENEMANAGER_HPP

#include <chrono>
#include <memory>

#include <guayaba/Core/NodeRef.hpp>
#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Terminal/IOSTerminal.hpp>

namespace guayaba {

class Node;

/*
    Owns the active scene root and runs the frame loop.

    change_scene() takes a unique_ptr, so the hand-off of a root is visible in the
    type rather than being a matter of discipline. take_scene() gives the root
    back if you would rather keep it alive across a scene change.
*/
class SceneManager {
    private:
        std::unique_ptr<Node> root_node;

        bool is_running = false;
        SceneManager() = default;
        SceneManager(const SceneManager&) = delete;
        SceneManager& operator=(const SceneManager&) = delete;

        std::chrono::high_resolution_clock::time_point m_lastTime;
        double m_deltaTime = 0.0;

        unsigned long int frame_count = 0;

        std::unique_ptr<Node> next_scene;

        std::unique_ptr<IOSTerminal> os_terminal;

    public:
        static SceneManager& get_instance();

        void process_scene();

// Non-owning. Empty once the scene is gone - including when the scene is
    // swapped out from under a handle you were holding.
    NodeRef<Node> get_current_scene();

        // The manager takes ownership.
        void change_scene(std::unique_ptr<Node> new_root);

        // Hands the current root back to the caller, who becomes responsible for
        // destroying it. The scene becomes null until the next change_scene().
        std::unique_ptr<Node> take_scene();

        void start_running();

        void stop_running();

        unsigned long int get_frame_count() const;

        void increase_frame_count();
};

} // namespace guayaba

#endif // GUAYABA_SCENE_SCENEMANAGER_HPP