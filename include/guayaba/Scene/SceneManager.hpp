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

        // Frame rate cap. 0 means uncapped: the loop runs as fast as it can.
        int target_fps = 0;

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

        /*
            Frame rate cap for the loop. 0 (the default) leaves it uncapped, for a
            scene that measures its own frame time. A positive value makes the loop
            sleep out the unused part of each frame's budget, so a scene that only
            changes on an interval does not burn a core redrawing unchanged frames.

            Set before start_running() (or any time from a running scene). Safe
            to change between frames; the new value is read on the next one.
        */
        void set_target_fps(int fps);
        int get_target_fps() const;

        unsigned long int get_frame_count() const;

        void increase_frame_count();
};

} // namespace guayaba

#endif // GUAYABA_SCENE_SCENEMANAGER_HPP