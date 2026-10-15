// Headless checks for the LineEdit control: typing, the editing keys, the caret,
// max_length, signals, secret mode and focus. No terminal; InputEvents are
// synthesized and fed straight to Node::route_input, the same route
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

InputEvent key_press(Key key, char32_t text = 0) {
    InputEvent event;
    event.type = InputEvent::Type::KEY;
    event.pressed = true;
    event.key = key;
    event.text = text;
    return event;
}

InputEvent mouse_press(Vec2 pos) {
    InputEvent event;
    event.type = InputEvent::Type::MOUSE_BUTTON;
    event.pressed = true;
    event.mouse_button = MouseButton::LEFT;
    event.mouse_pos = pos;
    return event;
}

// Routes one event through a root and reports whether a control consumed it.
bool route(Node& root, const InputEvent& event) {
    std::vector<InputEvent> events{event};
    root.route_input(events);
    return events.empty();
}

void type(Node& root, Key key, char32_t text) {
    assert(route(root, key_press(key, text)));
}

void type_ascii(Node& root, char c) {
    // Letters are their ASCII codes on Key, which is what the field gets from the
    // backend for a plain keypress.
    type(root, static_cast<Key>(static_cast<uint16_t>(c)), static_cast<char32_t>(c));
}

// Builds a minimal scene: one field, laid out against the screen and focused.
struct Fixture {
    std::unique_ptr<Node> root = std::make_unique<Node>("root");
    NodeRef<LineEdit> field;

    Fixture() {
        FocusManager::get_instance().clear();
        field = root->add_child<LineEdit>("field", Vec2{0, 0});
        field->set_size({20, 3});
        root->update_layout(Rect::from_size(SCREEN));
    }

    ~Fixture() { FocusManager::get_instance().clear(); }

    LineEdit* operator->() const { return field.get(); }
};

void test_typing_and_changed_signal() {
    Fixture scene;
    scene->grab_focus();
    assert(scene->is_focused());

    int changed = 0;
    std::string last;
    scene->connect_signal<std::string>("text_changed", [&](const std::string& value) {
        ++changed;
        last = value;
    });

    type_ascii(*scene.root, 'h');
    type_ascii(*scene.root, 'i');
    assert(scene->get_text() == "hi");
    assert(scene->get_caret() == 2);
    assert(changed == 2);
    assert(last == "hi");
}

void test_keys_gated_by_focus() {
    Fixture scene;
    assert(!scene->is_focused());

    // Not focused: the field declines the key, so it stays for whoever comes next.
    assert(!route(*scene.root, key_press(Key::A, U'a')));
    assert(scene->get_text().empty());
}

void test_click_takes_focus() {
    Fixture scene;
    assert(!scene->is_focused());

    assert(route(*scene.root, mouse_press(Vec2{2, 1})));
    assert(scene->is_focused());

    // A press outside the field is not this control's to take.
    scene->release_focus();
    assert(!route(*scene.root, mouse_press(Vec2{50, 1})));
    assert(!scene->is_focused());
}

void test_backspace_delete() {
    Fixture scene;
    scene->grab_focus();

    scene->set_text("abc");
    scene->set_caret(3);
    assert(route(*scene.root, key_press(Key::BACKSPACE)));
    assert(scene->get_text() == "ab");
    assert(scene->get_caret() == 2);

    // Delete removes at the caret and leaves it where it was.
    scene->set_caret(1);
    assert(route(*scene.root, key_press(Key::DELETE)));
    assert(scene->get_text() == "a");
    assert(scene->get_caret() == 1);

    // At the ends there is nothing to erase, but the key is still consumed.
    assert(route(*scene.root, key_press(Key::BACKSPACE)));
    assert(scene->get_text().empty());
    assert(route(*scene.root, key_press(Key::DELETE)));
    assert(scene->get_text().empty());
}

void test_caret_movement() {
    Fixture scene;
    scene->grab_focus();
    scene->set_text("abcd");
    scene->set_caret(4);

    assert(route(*scene.root, key_press(Key::HOME)));
    assert(scene->get_caret() == 0);

    assert(route(*scene.root, key_press(Key::RIGHT)));
    assert(scene->get_caret() == 1);

    assert(route(*scene.root, key_press(Key::LEFT)));
    assert(scene->get_caret() == 0);

    // At the start, left does not move past it.
    assert(route(*scene.root, key_press(Key::LEFT)));
    assert(scene->get_caret() == 0);

    assert(route(*scene.root, key_press(Key::END)));
    assert(scene->get_caret() == 4);

    // At the end, right does not move past it.
    assert(route(*scene.root, key_press(Key::RIGHT)));
    assert(scene->get_caret() == 4);
}

// The caret is a byte offset, so the movement and erase keys have to step whole
// codepoints. "e-acute a": 0xC3 0xA9 then 'a'.
void test_caret_steps_codepoints() {
    Fixture scene;
    scene->grab_focus();
    scene->set_text("\xC3\xA9" "a");
    assert(scene->get_text().size() == 3);

    scene->set_caret(0);
    assert(route(*scene.root, key_press(Key::RIGHT)));
    assert(scene->get_caret() == 2);  // past the two-byte codepoint, not one byte
    assert(route(*scene.root, key_press(Key::RIGHT)));
    assert(scene->get_caret() == 3);
    assert(route(*scene.root, key_press(Key::LEFT)));
    assert(scene->get_caret() == 2);
    assert(route(*scene.root, key_press(Key::LEFT)));
    assert(scene->get_caret() == 0);

    // An index inside a codepoint snaps back to its lead byte.
    scene->set_caret(1);
    assert(scene->get_caret() == 0);

    // Delete at the start removes the whole two-byte codepoint.
    assert(route(*scene.root, key_press(Key::DELETE)));
    assert(scene->get_text() == "a");

    // Backspace behind the caret removes the codepoint before it.
    scene->set_text("\xC3\xA9" "a");
    scene->set_caret(3);
    assert(route(*scene.root, key_press(Key::BACKSPACE)));
    assert(scene->get_text() == "\xC3\xA9");
}

void test_max_length_counts_codepoints() {
    Fixture scene;
    scene->grab_focus();
    scene->set_max_length(3);
    assert(scene->get_max_length() == 3);

    type_ascii(*scene.root, 'a');
    type_ascii(*scene.root, 'b');
    type_ascii(*scene.root, 'c');
    type_ascii(*scene.root, 'd');  // dropped, but still consumed
    assert(scene->get_text() == "abc");

    // Two two-byte codepoints hit a limit of two, not a limit of four bytes.
    scene->clear();
    scene->set_text("");
    scene->set_max_length(2);
    type(*scene.root, Key::UNKNOWN, 0xE9);
    type(*scene.root, Key::UNKNOWN, 0xE9);
    type(*scene.root, Key::UNKNOWN, 0xE9);
    assert(scene->get_text().size() == 4);
    assert(scene->get_text() == "\xC3\xA9\xC3\xA9");

    // Backspace makes room again.
    assert(route(*scene.root, key_press(Key::BACKSPACE)));
    type(*scene.root, Key::UNKNOWN, 0xE9);
    assert(scene->get_text().size() == 4);
}

void test_submit_signal() {
    Fixture scene;
    scene->grab_focus();
    scene->set_text("go");

    int submitted = 0;
    std::string last;
    scene->connect_signal<std::string>("text_submitted", [&](const std::string& value) {
        ++submitted;
        last = value;
    });

    assert(route(*scene.root, key_press(Key::ENTER)));
    assert(submitted == 1);
    assert(last == "go");

    // Enter submits; it does not change the text, so text_changed stays quiet.
    assert(scene->get_text() == "go");
}

void test_set_text_clear_and_changed_signal() {
    Fixture scene;
    scene->grab_focus();

    int changed = 0;
    scene->connect_signal<std::string>("text_changed", [&](const std::string&) { ++changed; });

    scene->set_text("one");
    assert(changed == 1);
    scene->set_text("one");  // unchanged: no signal
    assert(changed == 1);

    scene->clear();
    assert(changed == 2);
    assert(scene->get_text().empty());
    assert(scene->get_caret() == 0);

    scene->clear();  // already empty: no signal
    assert(changed == 2);
}

void test_tab_and_escape_are_declined() {
    Fixture scene;
    scene->grab_focus();

    // Declined so the scene keeps them: Tab moves focus, Escape cancels.
    assert(!route(*scene.root, key_press(Key::TAB, U'\t')));
    assert(!route(*scene.root, key_press(Key::ESCAPE)));
}

void test_secret_does_not_change_text() {
    Fixture scene;
    scene->grab_focus();
    scene->set_secret(true);
    assert(scene->is_secret());

    scene->set_text("hunter2");
    scene->set_secret_character(U'#');

    assert(scene->get_text() == "hunter2");
    assert(scene->get_caret() <= scene->get_text().size());
}

// Drives draw() over a synthesized event sequence: scrolling, the caret and
// secret mode all live in this path, and a crash here would not show up in the
// editing checks above. The renderer has no buffer accessor, so this is a smoke
// run rather than a pixel assertion.
void test_draw_smoke() {
    Fixture scene;
    scene->set_size({6, 3});  // narrower than the text, so the window scrolls
    scene.root->update_layout(Rect::from_size(SCREEN));
    scene->grab_focus();
    scene->set_text("a longer line of text");
    scene->set_caret(scene->get_text().size());  // scroll the window to the caret
    scene->set_placeholder("type here");

    ConsoleRenderer renderer(SCREEN, nullptr);
    scene->draw(renderer, Rect::from_size(SCREEN));

    scene->set_secret(true);
    scene->draw(renderer, Rect::from_size(SCREEN));

    // Empty with a placeholder, plus the caret block at column zero.
    scene->set_text("");
    scene->draw(renderer, Rect::from_size(SCREEN));

    // Unfocused: no caret, placeholder only.
    scene->release_focus();
    scene->draw(renderer, Rect::from_size(SCREEN));
}

} // namespace

int main() {
    test_typing_and_changed_signal();
    test_keys_gated_by_focus();
    test_click_takes_focus();
    test_backspace_delete();
    test_caret_movement();
    test_caret_steps_codepoints();
    test_max_length_counts_codepoints();
    test_submit_signal();
    test_set_text_clear_and_changed_signal();
    test_tab_and_escape_are_declined();
    test_secret_does_not_change_text();
    test_draw_smoke();

    std::cout << "line_edit: all checks passed." << std::endl;
    return 0;
}
