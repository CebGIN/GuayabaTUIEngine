// Headless checks for the ListView control: item replacement, selection
// clamping, the change/activation signals, the scroll window following the
// cursor, wheel and click handling, and a draw smoke pass. No terminal; events
// are synthesized and fed through Node::route_input, the same route
// tests/input_routing.cpp uses.

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <guayaba/Guayaba.hpp>

using namespace guayaba;

namespace {

constexpr Vec2 SCREEN{80, 24};

InputEvent key_press(Key key) {
    InputEvent event;
    event.type = InputEvent::Type::KEY;
    event.pressed = true;
    event.key = key;
    return event;
}

InputEvent mouse_button(Vec2 pos, bool pressed) {
    InputEvent event;
    event.type = InputEvent::Type::MOUSE_BUTTON;
    event.pressed = pressed;
    event.mouse_button = MouseButton::LEFT;
    event.mouse_pos = pos;
    return event;
}

InputEvent wheel(Vec2 pos, int delta) {
    InputEvent event;
    event.type = InputEvent::Type::MOUSE_WHEEL;
    event.wheel_delta = delta;
    event.mouse_pos = pos;
    return event;
}

// Routes one event through a root and reports whether a control consumed it.
bool route(Node& root, const InputEvent& event) {
    std::vector<InputEvent> events{event};
    root.route_input(events);
    return events.empty();
}

// One list, ten rows tall three: the window and the clamping are all visible in
// the numbers.
struct Fixture {
    std::unique_ptr<Node> root = std::make_unique<Node>("root");
    NodeRef<ListView> list;

    Fixture() {
        FocusManager::get_instance().clear();
        list = root->add_child<ListView>("list", Vec2{0, 0});
        list->set_size({10, 3});
        root->update_layout(Rect::from_size(SCREEN));
        list->set_items({"a", "b", "c", "d", "e"});
    }

    ~Fixture() { FocusManager::get_instance().clear(); }

    ListView* operator->() const { return list.get(); }
};

void test_items_and_selection_clamping() {
    Fixture scene;
    assert(scene->item_count() == 5);
    assert(scene->get_items()[2] == "c");
    assert(scene->selected() == -1);  // nothing selected to begin with

    scene->set_selected(10);
    assert(scene->selected() == 4);  // clamped to the last row

    scene->set_selected(-1);
    assert(scene->selected() == -1);

    scene->set_selected(-7);
    assert(scene->selected() == -1);

    // An empty list always answers -1.
    scene->set_items({});
    scene->set_selected(3);
    assert(scene->selected() == -1);
    assert(scene->item_count() == 0);
}

void test_move_selection_and_signal() {
    Fixture scene;

    int changes = 0;
    int last_changed = -2;
    scene->connect_signal<int>("selection_changed", [&](int index) {
        ++changes;
        last_changed = index;
    });

    // From nothing selected, moving forward enters at the first row.
    scene->move_selection(1);
    assert(scene->selected() == 0);
    assert(changes == 1 && last_changed == 0);

    scene->move_selection(1);
    assert(scene->selected() == 1);
    assert(changes == 2);

    // A large step clamps at the last row.
    scene->move_selection(10);
    assert(scene->selected() == 4);
    assert(changes == 3);

    // At the end, moving further changes nothing and stays silent.
    scene->move_selection(1);
    assert(scene->selected() == 4);
    assert(changes == 3);

    scene->move_selection(-100);
    assert(scene->selected() == 0);
    assert(changes == 4);
}

void test_scroll_follows_cursor() {
    Fixture scene;
    assert(scene->visible_count() == 3);
    assert(scene->scroll() == 0);

    // The cursor at the bottom pulls the window down by exactly enough.
    scene->select_last();
    assert(scene->selected() == 4);
    assert(scene->scroll() == 2);  // rows 2,3,4

    scene->select_first();
    assert(scene->selected() == 0);
    assert(scene->scroll() == 0);

    // Walking down one at a time only scrolls when the cursor leaves the window.
    scene->set_selected(2);
    assert(scene->scroll() == 0);
    scene->move_selection(1);
    assert(scene->selected() == 3);
    assert(scene->scroll() == 1);

    // set_scroll clamps into range.
    scene->set_scroll(100);
    assert(scene->scroll() == 2);
    scene->set_scroll(-5);
    assert(scene->scroll() == 0);
}

void test_home_end_and_paging() {
    Fixture scene;
    scene->grab_focus();

    scene->set_selected(0);
    assert(route(*scene.root, key_press(Key::END)));
    assert(scene->selected() == 4);
    assert(scene->scroll() == 2);

    assert(route(*scene.root, key_press(Key::HOME)));
    assert(scene->selected() == 0);
    assert(scene->scroll() == 0);

    // A page is visible_count - 1, so one row of the old page stays for context.
    assert(route(*scene.root, key_press(Key::PAGE_DOWN)));
    assert(scene->selected() == 2);
    assert(route(*scene.root, key_press(Key::PAGE_UP)));
    assert(scene->selected() == 0);
}

void test_wheel_scrolls_but_does_not_move_cursor() {
    Fixture scene;
    scene->set_selected(1);

    // One notch down: the window moves, the cursor does not.
    assert(route(*scene.root, wheel(Vec2{2, 1}, -1)));
    assert(scene->scroll() == 1);
    assert(scene->selected() == 1);

    assert(route(*scene.root, wheel(Vec2{2, 1}, 1)));
    assert(scene->scroll() == 0);
    assert(scene->selected() == 1);

    // Nothing to scroll: the notch is declined so an enclosing scroll view can
    // take it.
    scene->set_size({10, 10});
    scene.root->update_layout(Rect::from_size(SCREEN));
    assert(!route(*scene.root, wheel(Vec2{2, 1}, -1)));
    assert(scene->scroll() == 0);
}

void test_activation() {
    Fixture scene;
    scene->grab_focus();

    int activated = 0;
    int last = -2;
    scene->connect_signal<int>("item_activated", [&](int index) {
        ++activated;
        last = index;
    });

    // ENTER with nothing selected does nothing.
    assert(route(*scene.root, key_press(Key::ENTER)));
    assert(activated == 0);

    scene->set_selected(3);
    assert(route(*scene.root, key_press(Key::ENTER)));
    assert(activated == 1);
    assert(last == 3);
}

void test_click_selects_and_takes_focus() {
    Fixture scene;
    assert(!scene->is_focused());
    assert(scene->selected() == -1);

    // The press focuses the list and selects the row under the pointer.
    assert(route(*scene.root, mouse_button(Vec2{2, 2}, true)));
    assert(scene->is_focused());
    assert(scene->selected() == 2);

    // The matching release is consumed as well.
    assert(route(*scene.root, mouse_button(Vec2{2, 2}, false)));

    // A click while already focused is still a selection.
    assert(route(*scene.root, mouse_button(Vec2{2, 0}, true)));
    assert(scene->selected() == 0);
}

void test_tab_and_escape_are_declined() {
    Fixture scene;
    scene->grab_focus();
    scene->set_selected(0);

    assert(!route(*scene.root, key_press(Key::TAB)));
    assert(!route(*scene.root, key_press(Key::ESCAPE)));
}

void test_items_replace_clamps_cursor_and_window() {
    Fixture scene;
    scene->select_last();
    assert(scene->selected() == 4);
    assert(scene->scroll() == 2);

    scene->set_items({"x", "y"});
    assert(scene->item_count() == 2);
    assert(scene->selected() == 1);  // clamped into the new range
    assert(scene->scroll() == 0);    // there is no window left to scroll
}

void test_draw_smoke() {
    Fixture scene;
    scene->set_selected(1);
    scene->set_item_color(2, Color::from_name("RED"));
    scene->set_selection_background_color("BRIGHT_BLACK");
    scene->set_selection_text_color("BRIGHT_WHITE");

    ConsoleRenderer renderer(SCREEN, nullptr);
    scene->draw(renderer, Rect::from_size(SCREEN));

    // Non-ASCII rows, which the per-cell decoder has to step whole codepoints to
    // keep the columns aligned.
    scene->set_items({"\xC3\xA9" "tude", "caf\xC3\xA9", "\xE2\x82\xAC" "uro"});
    scene->set_selected(0);
    scene->draw(renderer, Rect::from_size(SCREEN));

    // A copy that fits without scrolling, and an empty list.
    scene->set_size({10, 10});
    scene.root->update_layout(Rect::from_size(SCREEN));
    scene->draw(renderer, Rect::from_size(SCREEN));

    scene->set_items({});
    scene->draw(renderer, Rect::from_size(SCREEN));
}

} // namespace

int main() {
    test_items_and_selection_clamping();
    test_move_selection_and_signal();
    test_scroll_follows_cursor();
    test_home_end_and_paging();
    test_wheel_scrolls_but_does_not_move_cursor();
    test_activation();
    test_click_selects_and_takes_focus();
    test_tab_and_escape_are_declined();
    test_items_replace_clamps_cursor_and_window();
    test_draw_smoke();

    std::cout << "listview: all checks passed." << std::endl;
    return 0;
}
