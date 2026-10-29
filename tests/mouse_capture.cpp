// Headless checks for CORE-03: one control can take the mouse, sees mouse events
// before the tree even outside its rect, and loses the capture when it is
// destroyed, hidden or the scene changes.
//
// The capture dispatch is a static on Control, so these tests call it directly,
// exactly as SceneManager::run_frame does before routing the tree.

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <guayaba/Guayaba.hpp>

using namespace guayaba;

namespace {

constexpr Vec2 SCREEN{40, 12};

class Probe : public Control {
public:
    Probe(const std::string& node_name, Vec2 node_position)
        : Control(node_name, node_position) {}

    int handled = 0;
    bool consume = false;

protected:
    bool on_gui_input(const InputEvent&) override {
        ++handled;
        return consume;
    }
};

InputEvent mouse_button(Vec2 pos) {
    InputEvent event;
    event.type = InputEvent::Type::MOUSE_BUTTON;
    event.pressed = true;
    event.mouse_button = MouseButton::LEFT;
    event.mouse_pos = pos;
    return event;
}

InputEvent mouse_motion(Vec2 pos) {
    InputEvent event;
    event.type = InputEvent::Type::MOUSE_MOTION;
    event.mouse_pos = pos;
    return event;
}

InputEvent key_press(Key key) {
    InputEvent event;
    event.type = InputEvent::Type::KEY;
    event.pressed = true;
    event.key = key;
    return event;
}

void reset_all() {
    SceneManager::get_instance().take_scene();
    if (Control* captor = Control::get_mouse_capture()) captor->release_mouse();
    FocusManager::get_instance().clear();
}

// The whole point: a motion outside the rect still reaches the captor.
void test_capture_receives_outside_rect() {
    reset_all();

    auto root = std::make_unique<Node>("root");
    NodeRef<Probe> probe = root->add_child<Probe>("probe", Vec2{0, 0});
    probe->set_size({5, 1});
    root->update_layout(Rect::from_size(SCREEN));

    probe->capture_mouse();
    assert(probe->has_mouse_capture());
    assert(Control::get_mouse_capture() == probe.get());

    std::vector<InputEvent> events{mouse_motion({20, 10})};
    Control::route_captured_input(events);

    assert(probe->handled == 1);
}

// A consumed event never reaches the control under the pointer.
void test_consumed_event_does_not_reach_the_tree() {
    reset_all();

    auto root = std::make_unique<Node>("root");
    NodeRef<Probe> probe = root->add_child<Probe>("probe", Vec2{0, 0});
    NodeRef<Probe> other = root->add_child<Probe>("other", Vec2{10, 5});
    probe->set_size({5, 1});
    other->set_size({5, 2});
    root->update_layout(Rect::from_size(SCREEN));

    probe->consume = true;
    probe->capture_mouse();

    std::vector<InputEvent> events{mouse_button({11, 6})};
    Control::route_captured_input(events);

    assert(events.empty());
    assert(probe->handled == 1);
    assert(other->handled == 0);
}

// A declined event continues through the normal walk and reaches the control
// under the pointer.
void test_declined_event_continues() {
    reset_all();

    auto root = std::make_unique<Node>("root");
    NodeRef<Probe> probe = root->add_child<Probe>("probe", Vec2{0, 0});
    NodeRef<Probe> other = root->add_child<Probe>("other", Vec2{10, 5});
    probe->set_size({5, 1});
    other->set_size({5, 2});
    root->update_layout(Rect::from_size(SCREEN));

    probe->capture_mouse();
    other->consume = true;

    std::vector<InputEvent> events{mouse_button({11, 6})};
    Control::route_captured_input(events);
    assert(events.size() == 1);

    root->route_input(events);
    assert(probe->handled == 1);
    assert(other->handled == 1);
    assert(events.empty());
}

// Capture is a mouse contract only: keys still go to the focused control.
void test_keyboard_is_not_captured() {
    reset_all();

    auto root = std::make_unique<Node>("root");
    NodeRef<Probe> probe = root->add_child<Probe>("probe", Vec2{0, 0});
    probe->set_size({5, 1});
    root->update_layout(Rect::from_size(SCREEN));

    probe->capture_mouse();

    std::vector<InputEvent> events{key_press(Key::Q)};
    Control::route_captured_input(events);

    assert(probe->handled == 0);
    assert(events.size() == 1);
}

// The authority is held by id: destroying the captor drops it, with no dangling
// pointer.
void test_destruction_releases_capture() {
    reset_all();

    auto root = std::make_unique<Node>("root");
    NodeRef<Probe> probe = root->add_child<Probe>("probe", Vec2{0, 0});
    Probe* raw = probe.get();

    raw->capture_mouse();
    assert(Control::get_mouse_capture() == raw);

    // Removing it hands ownership over; the node is still alive until the
    // unique_ptr goes away.
    std::unique_ptr<Node> owned = root->remove_child(0);
    assert(Control::get_mouse_capture() == raw);

    owned.reset();
    assert(Control::get_mouse_capture() == nullptr);
}

void test_hiding_releases_capture() {
    reset_all();

    auto root = std::make_unique<Node>("root");
    NodeRef<Probe> probe = root->add_child<Probe>("probe", Vec2{0, 0});
    probe->set_size({5, 1});

    probe->capture_mouse();
    assert(probe->has_mouse_capture());

    probe->set_visible(false);
    assert(Control::get_mouse_capture() == nullptr);
}

void test_hidden_control_cannot_capture() {
    reset_all();

    auto root = std::make_unique<Node>("root");
    NodeRef<Probe> probe = root->add_child<Probe>("probe", Vec2{0, 0});

    probe->set_visible(false);
    probe->capture_mouse();

    assert(Control::get_mouse_capture() == nullptr);
    assert(!probe->has_mouse_capture());
}

void test_capture_replaces_previous_holder() {
    reset_all();

    auto root = std::make_unique<Node>("root");
    NodeRef<Probe> a = root->add_child<Probe>("a", Vec2{0, 0});
    NodeRef<Probe> b = root->add_child<Probe>("b", Vec2{10, 0});

    a->capture_mouse();
    b->capture_mouse();
    assert(Control::get_mouse_capture() == b.get());

    // Releasing a control that no longer holds it does nothing.
    a->release_mouse();
    assert(Control::get_mouse_capture() == b.get());

    b->release_mouse();
    assert(Control::get_mouse_capture() == nullptr);
}

void test_scene_change_clears_capture() {
    reset_all();

    auto root = std::make_unique<Node>("root");
    NodeRef<Probe> probe = root->add_child<Probe>("probe", Vec2{0, 0});
    probe->set_size({5, 1});
    SceneManager::get_instance().change_scene(std::move(root));

    ConsoleRenderer renderer(SCREEN, nullptr);
    SceneManager::get_instance().run_frame(renderer, {});
    assert(probe.get() != nullptr);

    probe->capture_mouse();
    assert(Control::get_mouse_capture() == probe.get());

    SceneManager::get_instance().change_scene(std::make_unique<Node>("next"));
    SceneManager::get_instance().run_frame(renderer, {});

    assert(Control::get_mouse_capture() == nullptr);
}

} // namespace

int main() {
    test_capture_receives_outside_rect();
    test_consumed_event_does_not_reach_the_tree();
    test_declined_event_continues();
    test_keyboard_is_not_captured();
    test_destruction_releases_capture();
    test_hiding_releases_capture();
    test_hidden_control_cannot_capture();
    test_capture_replaces_previous_holder();
    test_scene_change_clears_capture();

    std::cout << "mouse_capture: all checks passed." << std::endl;
    return 0;
}
