// Headless checks for CORE-01: hiding a subtree stops its frame phases, removes
// it from container minimums and from the focus walk, and releases the focus only
// when it was inside that subtree.
//
// The probes call the public wrappers directly in some tests, which is the point:
// a hidden node must be skipped even when someone calls update()/draw() on it
// without going through the frame loop.

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <guayaba/Guayaba.hpp>

using namespace guayaba;

namespace {

constexpr Vec2 SCREEN{40, 12};

// Counts the phases that reach it. The frame loop is not required: a direct
// wrapper call must be cut just the same.
class Probe : public Control {
public:
    explicit Probe(const std::string& node_name = "Probe") : Control(node_name) {}

    int updates = 0;
    int layouts = 0;
    int draws = 0;
    int gui_inputs = 0;

    void on_update(double delta_time) override {
        ++updates;
        Node::on_update(delta_time);
    }

    void on_update_layout(const Rect& parent_rect) override {
        ++layouts;
        Control::on_update_layout(parent_rect);
    }

    void on_draw(ConsoleRenderer& renderer, const Rect& clip) override {
        ++draws;
        Control::on_draw(renderer, clip);
    }

protected:
    bool on_gui_input(const InputEvent&) override {
        ++gui_inputs;
        return true;
    }
};

class ProcessProbe : public Script<Node> {
public:
    int processed = 0;

    void on_process(double) override { ++processed; }
};

InputEvent click(Vec2 pos) {
    InputEvent event;
    event.type = InputEvent::Type::MOUSE_BUTTON;
    event.pressed = true;
    event.mouse_button = MouseButton::LEFT;
    event.mouse_pos = pos;
    return event;
}

void reset_focus() {
    FocusManager::get_instance().clear();
}

void test_visibility_flags() {
    auto root = std::make_unique<Node>("root");
    NodeRef<Node> child = root->add_child<Node>("child");
    NodeRef<Node> grandchild = child->add_child<Node>("grandchild");

    assert(root->is_visible());
    assert(root->is_visible_in_tree());
    assert(grandchild->is_visible_in_tree());

    // Hiding a parent hides the whole subtree, without touching the flags below it.
    child->set_visible(false);
    assert(!child->is_visible());
    assert(!child->is_visible_in_tree());
    assert(grandchild->is_visible());
    assert(!grandchild->is_visible_in_tree());
    assert(root->is_visible_in_tree());

    child->set_visible(true);
    assert(grandchild->is_visible_in_tree());

    assert(root->is_ancestor_of(child.get()));
    assert(root->is_ancestor_of(grandchild.get()));
    assert(root->is_ancestor_of(root.get()));
    assert(!child->is_ancestor_of(root.get()));
}

// on_process is driven by Node::update, so a hidden subtree never reaches it.
void test_hidden_does_not_process() {
    auto root = std::make_unique<Node>("root");
    ScriptRef<Node, ProcessProbe> root_script = root->attach_script<ProcessProbe>();
    NodeRef<Node> child = root->add_child<Node>("child");
    ScriptRef<Node, ProcessProbe> child_script = child->attach_script<ProcessProbe>();

    child->set_visible(false);
    root->update(0.016);

    assert(root_script.script()->processed == 1);
    assert(child_script.script()->processed == 0);

    child->set_visible(true);
    root->update(0.016);
    assert(child_script.script()->processed == 1);
}

void test_hidden_skips_layout_and_draw() {
    auto root = std::make_unique<Node>("root");
    NodeRef<Probe> probe = root->add_child<Probe>("probe");
    probe->set_size({5, 1});

    ConsoleRenderer renderer(SCREEN, nullptr);
    const Rect screen = Rect::from_size(SCREEN);

    probe->set_visible(false);
    root->update_layout(screen);
    root->draw(renderer, screen);
    assert(probe->layouts == 0);
    assert(probe->draws == 0);

    probe->set_visible(true);
    root->update_layout(screen);
    root->draw(renderer, screen);
    assert(probe->layouts == 1);
    assert(probe->draws == 1);
}

// A hidden control ignores input even when route_input is called on it directly.
void test_hidden_skips_input_and_update() {
    auto root = std::make_unique<Node>("root");
    NodeRef<Probe> probe = root->add_child<Probe>("probe");
    probe->set_size({5, 1});
    root->update_layout(Rect::from_size(SCREEN));

    probe->set_visible(false);

    std::vector<InputEvent> events{click({1, 1})};
    probe->route_input(events);
    probe->update(0.016);

    assert(probe->gui_inputs == 0);
    assert(probe->updates == 0);
    assert(events.size() == 1);
}

// Hiding a subtree releases a focus inside it, but leaves an unrelated focus
// alone. Showing it again does not steal the focus back.
void test_focus_release_is_targeted() {
    reset_focus();

    auto root = std::make_unique<Node>("root");
    NodeRef<Node> panel = root->add_child<Node>("panel");
    NodeRef<Probe> inside = panel->add_child<Probe>("inside");
    NodeRef<Probe> outside = root->add_child<Probe>("outside");

    inside->set_focus_mode(Control::FocusMode::CLICK);
    outside->set_focus_mode(Control::FocusMode::CLICK);

    inside->grab_focus();
    assert(inside->is_focused());

    panel->set_visible(false);
    assert(!inside->is_focused());
    assert(!outside->is_focused());

    // Hiding an unrelated control does not take the focus away.
    outside->grab_focus();
    assert(outside->is_focused());
    inside->set_visible(false);
    assert(outside->is_focused());

    // A hidden control cannot take the focus, by Tab or by name.
    inside->grab_focus();
    assert(!inside->is_focused());
    assert(outside->is_focused());

    reset_focus();
}

void test_focusable_excludes_hidden() {
    reset_focus();

    auto root = std::make_unique<Node>("root");
    NodeRef<Probe> shown = root->add_child<Probe>("shown");
    NodeRef<Probe> hidden = root->add_child<Probe>("hidden");
    shown->set_focus_mode(Control::FocusMode::CLICK);
    hidden->set_focus_mode(Control::FocusMode::CLICK);
    hidden->set_visible(false);

    std::vector<Control*> order = FocusManager::get_instance().get_focusable(root.get());
    assert(order.size() == 1);
    assert(order[0] == shown.get());

    reset_focus();
}

// A hidden child contributes nothing to the minimum and takes no cell.
void test_containers_ignore_hidden_children() {
    auto box = std::make_unique<HBox>("box");
    box->set_anchor_preset(AnchorPreset::FullRect);
    box->set_content_alignment(ContentAlignment::Begin);

    NodeRef<Probe> hidden = box->add_child<Probe>("hidden");
    NodeRef<Probe> shown = box->add_child<Probe>("shown");
    hidden->set_custom_minimum_size({6, 2});
    shown->set_custom_minimum_size({4, 1});

    assert(box->get_minimum_size().x == 10);
    assert(box->get_minimum_size().y == 2);

    hidden->set_visible(false);
    assert(box->get_minimum_size().x == 4);
    assert(box->get_minimum_size().y == 1);

    box->update_layout(Rect::from_size({20, 5}));
    assert(shown->get_rect().pos.x == 0);
    assert(hidden->layouts == 0);
}

// Hiding invalidates the cached rect; showing lays the node out again on the next
// pass, at the rect the new layout gives it.
void test_show_recalculates_layout() {
    auto root = std::make_unique<Node>("root");
    NodeRef<Probe> probe = root->add_child<Probe>("probe");
    probe->set_size({3, 1});

    const Rect screen = Rect::from_size(SCREEN);
    root->update_layout(screen);
    assert(probe->get_rect().size.x == 3);

    probe->set_visible(false);
    assert(probe->get_rect().size.x == 0);

    probe->set_visible(true);
    root->update_layout(screen);
    assert(probe->get_rect().size.x == 3);
}

// Hiding does not touch the script or the signal connections.
void test_signals_and_script_survive() {
    auto probe = std::make_unique<Probe>("probe");
    probe->attach_script<ProcessProbe>();
    probe->declare_signal("ping");

    int hits = 0;
    const uint64_t id = probe->connect_signal<>("ping", [&hits] { ++hits; });

    probe->set_visible(false);
    probe->emit_signal("ping");

    assert(hits == 1);
    assert(probe->is_connected(id));
    assert(probe->has_script());
}

} // namespace

int main() {
    test_visibility_flags();
    test_hidden_does_not_process();
    test_hidden_skips_layout_and_draw();
    test_hidden_skips_input_and_update();
    test_focus_release_is_targeted();
    test_focusable_excludes_hidden();
    test_containers_ignore_hidden_children();
    test_show_recalculates_layout();
    test_signals_and_script_survive();

    std::cout << "visibility: all checks passed." << std::endl;
    return 0;
}
