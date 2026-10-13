// Headless checks for Godot-style input routing: reverse-order delivery, focus
// gating, consumption, and the unhandled fallback. No terminal; events are
// synthesized and fed straight to Node::route_input.

#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

#include <guayaba/Guayaba.hpp>

using namespace guayaba;

namespace {

class TestControl : public Control {
public:
    TestControl(const std::string& name, Vec2 pos, std::vector<int>* order, int id)
        : Control(name, pos), order_(order), id_(id) {}

    bool consume = false;
    int handled = 0;

protected:
    bool on_gui_input(const InputEvent&) override {
        ++handled;
        if (order_ != nullptr) order_->push_back(id_);
        return consume;
    }

private:
    std::vector<int>* order_;
    int id_;
};

class TestScript : public Script<Node> {
public:
    int inputs = 0;
    int unhandled = 0;

    bool on_input(const InputEvent&) override {
        ++inputs;
        return false;  // never consume: leave it for the unhandled pass
    }
    void on_unhandled_input(const InputEvent&) override { ++unhandled; }
};

InputEvent mouse_press(Vec2 pos) {
    InputEvent event;
    event.type = InputEvent::Type::MOUSE_BUTTON;
    event.pressed = true;
    event.mouse_button = MouseButton::LEFT;
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

void test_reverse_order_and_consumption() {
    auto root = std::make_unique<Node>("root");
    std::vector<int> order;
    NodeRef<TestControl> a = root->add_child<TestControl>("a", Vec2{0, 0}, &order, 1);
    NodeRef<TestControl> b = root->add_child<TestControl>("b", Vec2{0, 0}, &order, 2);
    a.get()->set_size({10, 3});
    b.get()->set_size({10, 3});
    root->update_layout(Rect::from_size({80, 24}));

    // Neither consumes: both see the event, topmost (later child) first.
    std::vector<InputEvent> events{mouse_press({1, 1})};
    root->route_input(events);
    assert(order.size() == 2);
    assert(order[0] == 2 && order[1] == 1);

    // The top control consumes: the one behind never sees it.
    order.clear();
    a.get()->handled = 0;
    b.get()->handled = 0;
    b.get()->consume = true;
    std::vector<InputEvent> events2{mouse_press({1, 1})};
    root->route_input(events2);
    assert(events2.empty());
    assert(b.get()->handled == 1);
    assert(a.get()->handled == 0);
}

void test_focus_gating() {
    FocusManager::get_instance().clear();

    auto root = std::make_unique<Node>("root");
    NodeRef<TestControl> a = root->add_child<TestControl>("a", Vec2{0, 0}, nullptr, 1);
    NodeRef<TestControl> b = root->add_child<TestControl>("b", Vec2{0, 0}, nullptr, 2);
    a.get()->set_size({10, 3});
    b.get()->set_size({10, 3});
    root->update_layout(Rect::from_size({80, 24}));

    // A key goes only to the focused control. Focus A, and A consumes.
    a.get()->set_focus_mode(Control::FocusMode::CLICK);
    b.get()->set_focus_mode(Control::FocusMode::CLICK);
    a.get()->consume = true;
    a.get()->grab_focus();
    assert(a.get()->is_focused());
    assert(!b.get()->is_focused());

    std::vector<InputEvent> events{key_press(Key::Q)};
    root->route_input(events);
    assert(events.empty());
    assert(a.get()->handled == 1);
    assert(b.get()->handled == 0);

    FocusManager::get_instance().clear();
    assert(!a.get()->is_focused());
}

void test_unhandled_fallback() {
    auto root = std::make_unique<Node>("root");
    auto script_ref = root->attach_script<TestScript>();
    TestScript* script = script_ref.script();
    assert(script != nullptr);

    NodeRef<TestControl> a = root->add_child<TestControl>("a", Vec2{0, 0}, nullptr, 1);
    a.get()->set_size({10, 3});
    root->update_layout(Rect::from_size({80, 24}));

    // No control holds focus, so the key is not consumed and falls through.
    std::vector<InputEvent> events{key_press(Key::Q)};
    root->route_input(events);
    assert(events.size() == 1);  // nobody consumed it

    root->route_unhandled_input(events[0]);
    assert(script->unhandled == 1);
}

} // namespace

int main() {
    test_reverse_order_and_consumption();
    test_focus_gating();
    test_unhandled_fallback();

    std::cout << "input_routing: all checks passed." << std::endl;
    return 0;
}
