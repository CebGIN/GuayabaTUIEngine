// Headless checks for SceneManager::run_frame(): the order of the phases inside
// a frame, the frame counter, and when a pending scene change takes effect.
//
// No terminal is opened: a silent IOSTerminal stands in for the backend, events
// are synthesized, and the renderer is already sized. Built only when
// GUAYABA_BUILD_TESTS is ON.

#include <algorithm>
#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <guayaba/Guayaba.hpp>

using namespace guayaba;

namespace {

constexpr Vec2 SCREEN{40, 12};

// Records that a frame reached present(), and optionally logs it in the same
// sequence as the node phases.
class SilentTerminal : public IOSTerminal {
public:
    std::vector<std::string>* log = nullptr;
    int presented = 0;

    void init() override {}
    void restore() override {}
    void set_cursor_visible(bool) override {}
    void set_cursor_position(Vec2) override {}
    void present_buffer(const std::vector<TermChar>&, Vec2) override {
        ++presented;
        if (log != nullptr) log->push_back("present");
    }
    Vec2 get_screen_size(Vec2 fallback) const override { return fallback; }
    void poll_events() override {}
    Vec2 get_mouse_position() const override { return {0, 0}; }
    bool is_left_click_pressed() const override { return false; }
    bool is_right_click_pressed() const override { return false; }
    bool is_left_click_just_pressed() const override { return false; }
    bool is_right_click_just_pressed() const override { return false; }
    std::vector<InputEvent> take_input_events() override { return {}; }
    bool was_key_pressed(Key) const override { return false; }
    int get_wheel_delta() const override { return 0; }
    std::string get_line_input(Vec2) override { return {}; }
};

// Logs "<name>:<phase>" for every phase the frame loop drives. `consume` makes
// a routed mouse event disappear, which is how a control takes it.
class RecordingNode : public Control {
public:
    RecordingNode(std::string node_name, std::vector<std::string>* log)
        : Control(std::move(node_name)), log_(log) {}

    bool consume = false;

    void update_layout(const Rect& parent_rect) override {
        record("layout");
        Control::update_layout(parent_rect);
    }

    void route_input(std::vector<InputEvent>& events) override {
        record("input");
        Control::route_input(events);
    }

    void route_unhandled_input(const InputEvent& event) override {
        record("unhandled");
        Node::route_unhandled_input(event);
    }

    void update(double delta_time) override {
        record("update");
        Node::update(delta_time);
    }

    void draw(ConsoleRenderer& renderer, const Rect& clip) override {
        record("draw");
        Control::draw(renderer, clip);
    }

protected:
    bool on_gui_input(const InputEvent&) override {
        record("gui");
        return consume;
    }

    void enter_tree() override {
        record("enter");
        Node::enter_tree();
    }

    void exit_tree() override {
        record("exit");
        Node::exit_tree();
    }

private:
    void record(const char* phase) {
        log_->push_back(get_name() + ":" + phase);
    }

    std::vector<std::string>* log_;
};

// Requests a scene change from inside the frame, once.
class ChangeSceneScript : public Script<Node> {
public:
    std::unique_ptr<Node> next;
    bool fired = false;

    void on_process(double) override {
        if (fired) return;
        fired = true;
        SceneManager::get_instance().change_scene(std::move(next));
    }
};

InputEvent key_press(Key key) {
    InputEvent event;
    event.type = InputEvent::Type::KEY;
    event.pressed = true;
    event.key = key;
    return event;
}

InputEvent mouse_click(Vec2 pos) {
    InputEvent event;
    event.type = InputEvent::Type::MOUSE_BUTTON;
    event.pressed = true;
    event.mouse_button = MouseButton::LEFT;
    event.mouse_pos = pos;
    return event;
}

bool logged(const std::vector<std::string>& log, const std::string& entry) {
    return std::find(log.begin(), log.end(), entry) != log.end();
}

void reset_scene() {
    SceneManager::get_instance().take_scene();
    FocusManager::get_instance().clear();
}

// Layout before routing, routing before update, update before draw, draw before
// present. An unconsumed key walks the whole path.
void test_phase_order() {
    reset_scene();

    std::vector<std::string> log;
    SceneManager::get_instance().change_scene(std::make_unique<RecordingNode>("root", &log));

    SilentTerminal terminal;
    terminal.log = &log;
    ConsoleRenderer renderer(SCREEN, &terminal);

    SceneManager::get_instance().run_frame(renderer, {key_press(Key::Q)});

    const std::vector<std::string> expected = {
        "root:enter",
        "root:layout",
        "root:input",
        "root:unhandled",
        "root:update",
        "root:draw",
        "present",
    };
    assert(log == expected);
}

void test_frame_count_and_present() {
    reset_scene();

    std::vector<std::string> log;
    SceneManager::get_instance().change_scene(std::make_unique<RecordingNode>("root", &log));

    SilentTerminal terminal;
    ConsoleRenderer renderer(SCREEN, &terminal);

    const unsigned long int before = SceneManager::get_instance().get_frame_count();
    SceneManager::get_instance().run_frame(renderer, {});
    SceneManager::get_instance().run_frame(renderer, {});

    assert(SceneManager::get_instance().get_frame_count() == before + 2);
    assert(terminal.presented == 2);
}

// Without a scene there is nothing to lay out, draw or present, and the counter
// does not move.
void test_no_scene_does_nothing() {
    reset_scene();

    SilentTerminal terminal;
    ConsoleRenderer renderer(SCREEN, &terminal);

    const unsigned long int before = SceneManager::get_instance().get_frame_count();
    SceneManager::get_instance().run_frame(renderer, {key_press(Key::Q)});

    assert(SceneManager::get_instance().get_frame_count() == before);
    assert(terminal.presented == 0);
}

// A change requested between frames is applied before layout, so the new scene
// is laid out and drawn in the same frame.
void test_scene_change_between_frames() {
    reset_scene();

    std::vector<std::string> log;
    SceneManager::get_instance().change_scene(std::make_unique<RecordingNode>("a", &log));

    SilentTerminal terminal;
    terminal.log = &log;
    ConsoleRenderer renderer(SCREEN, &terminal);

    SceneManager::get_instance().run_frame(renderer, {});
    assert((log == std::vector<std::string>{
        "a:enter", "a:layout", "a:input", "a:update", "a:draw", "present"}));

    log.clear();
    SceneManager::get_instance().change_scene(std::make_unique<RecordingNode>("b", &log));
    SceneManager::get_instance().run_frame(renderer, {});

    const std::vector<std::string> expected = {
        "a:exit",
        "b:enter",
        "b:layout",
        "b:input",
        "b:update",
        "b:draw",
        "present",
    };
    assert(log == expected);
}

// A change requested from on_process cannot tear down the tree mid-frame: the
// old scene finishes its frame and the swap happens at the start of the next.
void test_scene_change_during_frame() {
    reset_scene();

    std::vector<std::string> log;
    auto root = std::make_unique<RecordingNode>("old", &log);
    ScriptRef<Node, ChangeSceneScript> script = root->attach_script<ChangeSceneScript>();
    script.script()->next = std::make_unique<RecordingNode>("new", &log);
    SceneManager::get_instance().change_scene(std::move(root));

    SilentTerminal terminal;
    ConsoleRenderer renderer(SCREEN, &terminal);

    SceneManager::get_instance().run_frame(renderer, {});
    assert(logged(log, "old:draw"));
    assert(!logged(log, "new:enter"));

    log.clear();
    SceneManager::get_instance().run_frame(renderer, {});
    assert(log.size() >= 2);
    assert(log[0] == "old:exit");
    assert(log[1] == "new:enter");
}

// An event a control consumes never reaches the unhandled pass.
void test_consumed_event_skips_unhandled() {
    reset_scene();

    std::vector<std::string> log;
    auto root = std::make_unique<RecordingNode>("root", &log);
    root->set_size({10, 3});
    root->consume = true;
    SceneManager::get_instance().change_scene(std::move(root));

    SilentTerminal terminal;
    ConsoleRenderer renderer(SCREEN, &terminal);

    SceneManager::get_instance().run_frame(renderer, {mouse_click({1, 1})});

    assert(logged(log, "root:gui"));
    assert(!logged(log, "root:unhandled"));
}

} // namespace

int main() {
    test_phase_order();
    test_frame_count_and_present();
    test_no_scene_does_nothing();
    test_scene_change_between_frames();
    test_scene_change_during_frame();
    test_consumed_event_skips_unhandled();

    std::cout << "frame_order: all checks passed." << std::endl;
    return 0;
}
