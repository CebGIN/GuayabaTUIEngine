// Headless checks for CORE-02: call_deferred() is FIFO, thread-safe, runs before
// layout, waits while there is no scene, and keeps the rest of the queue when a
// call throws.
//
// The drain is private; every check drives it through SceneManager::run_frame,
// which is the only caller in the engine.

#include <cassert>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include <guayaba/Guayaba.hpp>

using namespace guayaba;

namespace {

constexpr Vec2 SCREEN{40, 12};

void reset_scene() {
    SceneManager::get_instance().take_scene();
    FocusManager::get_instance().clear();
}

void install_scene() {
    SceneManager::get_instance().change_scene(std::make_unique<Node>("root"));
}

void run_frame() {
    ConsoleRenderer renderer(SCREEN, nullptr);
    SceneManager::get_instance().run_frame(renderer, {});
}

void test_fifo() {
    reset_scene();
    install_scene();

    std::vector<int> order;
    for (int i = 0; i < 3; ++i) {
        SceneManager::get_instance().call_deferred([&order, i] { order.push_back(i); });
    }

    run_frame();
    assert((order == std::vector<int>{0, 1, 2}));
}

// An empty std::function would throw bad_function_call if it were enqueued.
void test_empty_callable_is_ignored() {
    reset_scene();
    install_scene();

    bool ran = false;
    SceneManager::get_instance().call_deferred({});
    SceneManager::get_instance().call_deferred([&ran] { ran = true; });

    run_frame();
    assert(ran);
}

void test_call_queued_during_drain_waits_for_next_frame() {
    reset_scene();
    install_scene();

    bool first = false;
    bool second = false;
    SceneManager::get_instance().call_deferred([&] {
        first = true;
        SceneManager::get_instance().call_deferred([&second] { second = true; });
    });

    run_frame();
    assert(first);
    assert(!second);

    run_frame();
    assert(second);
}

void test_called_from_another_thread_runs_on_the_frame_thread() {
    reset_scene();
    install_scene();

    const std::thread::id frame_thread = std::this_thread::get_id();
    std::thread::id seen{};

    std::thread worker([&seen] {
        SceneManager::get_instance().call_deferred([&seen] {
            seen = std::this_thread::get_id();
        });
    });
    worker.join();

    assert(seen == std::thread::id{});
    run_frame();
    assert(seen == frame_thread);
}

void test_exception_requeues_the_rest() {
    reset_scene();
    install_scene();

    bool after = false;
    SceneManager::get_instance().call_deferred([] { throw std::runtime_error("boom"); });
    SceneManager::get_instance().call_deferred([&after] { after = true; });

    bool threw = false;
    try {
        run_frame();
    } catch (const std::runtime_error&) {
        threw = true;
    }

    assert(threw);
    assert(!after);

    // The queue is still usable and the remaining call was not lost.
    run_frame();
    assert(after);
}

void test_no_scene_does_not_drain() {
    reset_scene();

    bool ran = false;
    SceneManager::get_instance().call_deferred([&ran] { ran = true; });

    run_frame();
    assert(!ran);

    // The call waited for a scene and runs in the frame that installs it.
    install_scene();
    run_frame();
    assert(ran);
}

// The queue drains before layout, so a deferred resize is already in effect for
// the frame that runs it.
void test_deferred_runs_before_layout() {
    reset_scene();

    auto root = std::make_unique<Node>("root");
    NodeRef<Control> control = root->add_child<Control>("control");
    control->set_size({2, 1});
    SceneManager::get_instance().change_scene(std::move(root));

    SceneManager::get_instance().call_deferred([control] {
        if (control) control->set_custom_minimum_size({7, 1});
    });

    run_frame();
    assert(control->get_rect().size.x == 7);
}

} // namespace

int main() {
    test_fifo();
    test_empty_callable_is_ignored();
    test_call_queued_during_drain_waits_for_next_frame();
    test_called_from_another_thread_runs_on_the_frame_thread();
    test_exception_requeues_the_rest();
    test_no_scene_does_not_drain();
    test_deferred_runs_before_layout();

    std::cout << "deferred_calls: all checks passed." << std::endl;
    return 0;
}
