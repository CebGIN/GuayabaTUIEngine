// Headless checks for RES-01: the empty theme is a no-op, applying one changes
// styles and colours, the walk reaches the whole tree, missing keys fall back,
// manual StyleBoxes win until the next apply, and margins affect layout.

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <guayaba/Guayaba.hpp>

using namespace guayaba;

namespace {

class CapturingTerminal : public IOSTerminal {
public:
    std::vector<TermChar> buffer;
    Vec2 size{0, 0};

    void init() override {}
    void restore() override {}
    void set_cursor_visible(bool) override {}
    void set_cursor_position(Vec2) override {}
    void present_buffer(const std::vector<TermChar>& presented, Vec2 presented_size) override {
        buffer = presented;
        size = presented_size;
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

    uint16_t foreground(int x, int y) const {
        return buffer[static_cast<std::size_t>(y) * size.x + x].foreground;
    }
    uint16_t background(int x, int y) const {
        return buffer[static_cast<std::size_t>(y) * size.x + x].background;
    }
};

void test_empty_theme_is_a_no_op() {
    Theme theme;
    assert(theme.revision() == 0);

    StyleBox fallback;
    fallback.set_background_color(7);

    const StyleBox& got = theme.get_style("missing", fallback);
    assert(&got == &fallback);
    assert(theme.get_color("missing", 9) == 9);
    assert(!theme.has_style("missing"));
    assert(!theme.has_color("missing"));

    Label label;
    apply_theme(label, theme);
    assert(!label.get_stylebox().has_background());
}

void test_label_style_and_text_color() {
    Theme theme;

    StyleBox box;
    box.set_background_color(4);
    theme.set_style("label", box);
    theme.set_color("text.primary", 3);

    Label label;
    label.set_text("x");
    label.set_size({2, 1});
    apply_theme(label, theme);
    assert(label.get_stylebox().get_background_color() == 4);

    CapturingTerminal terminal;
    ConsoleRenderer renderer({2, 1}, &terminal);
    label.update_layout(Rect::from_size({2, 1}));
    label.draw(renderer, Rect::from_size({2, 1}));
    renderer.present();

    assert(terminal.foreground(0, 0) == 3);
    // The cell with no text keeps the box background.
    assert(terminal.background(1, 0) == 4);
}

void test_button_states() {
    Theme theme;

    StyleBox normal;
    normal.set_background_color(1);
    StyleBox hover;
    hover.set_background_color(2);
    theme.set_style("button.normal", normal);
    theme.set_style("button.hover", hover);
    theme.set_color("text.disabled", 6);

    Button button;
    apply_theme(button, theme);

    assert(button.get_stylebox(ButtonState::Normal).get_background_color() == 1);
    assert(button.get_stylebox(ButtonState::Hover).get_background_color() == 2);
    assert(button.get_text_color(ButtonState::Disabled) == 6);
}

void test_apply_walks_the_whole_tree() {
    Theme theme;

    StyleBox panel_box;
    panel_box.set_background_color(2);
    StyleBox label_box;
    label_box.set_background_color(3);
    theme.set_style("panel", panel_box);
    theme.set_style("label", label_box);

    auto root = std::make_unique<Node>("root");
    NodeRef<PanelContainer> panel = root->add_child<PanelContainer>("panel");
    NodeRef<Label> label = panel->add_child<Label>("label");

    apply_theme(*root, theme);

    assert(panel->get_stylebox().get_background_color() == 2);
    assert(label->get_stylebox().get_background_color() == 3);
}

void test_revision_and_presence() {
    Theme theme;
    assert(theme.revision() == 0);

    theme.set_style("a", StyleBox{});
    assert(theme.revision() == 1);
    assert(theme.has_style("a"));

    theme.set_color("b", 1);
    assert(theme.revision() == 2);
    assert(theme.has_color("b"));
}

// A theme that changes the frame margins must be reflected by the next layout.
void test_margins_affect_layout() {
    Theme theme;

    StyleBox box;
    box.set_content_margin(2, 0, 2, 0);
    theme.set_style("label", box);

    Label label;
    label.set_text("abc");
    label.set_fit_content(true);
    assert(label.get_minimum_size().x == 3);

    apply_theme(label, theme);
    assert(label.get_minimum_size().x == 7);
}

// set_stylebox() is manual configuration and wins until the next apply_theme().
void test_manual_stylebox_wins_until_reapplied() {
    Theme theme;

    StyleBox themed;
    themed.set_background_color(2);
    theme.set_style("label", themed);

    Label label;
    apply_theme(label, theme);
    assert(label.get_stylebox().get_background_color() == 2);

    StyleBox manual;
    manual.set_background_color(5);
    label.set_stylebox(manual);
    assert(label.get_stylebox().get_background_color() == 5);

    apply_theme(label, theme);
    assert(label.get_stylebox().get_background_color() == 2);
}

// SpinBox adds its two arrow columns on top of whatever box the theme provides.
void test_spinbox_keeps_arrow_columns() {
    Theme theme;

    StyleBox box;
    box.set_content_margin(1, 0, 1, 0);
    theme.set_style("line_edit", box);

    SpinBox spin;
    apply_theme(spin, theme);

    const EdgeSize margin = spin.get_stylebox().get_content_margin();
    assert(margin.left == 1);
    assert(margin.right == 3);
}

void test_progress_bar_fill_color() {
    Theme theme;
    theme.set_color("progress_bar.fill", 5);

    ProgressBar bar;
    bar.set_range(0, 10);
    bar.set_value(10);
    bar.set_size({2, 1});
    apply_theme(bar, theme);

    CapturingTerminal terminal;
    ConsoleRenderer renderer({2, 1}, &terminal);
    bar.update_layout(Rect::from_size({2, 1}));
    bar.draw(renderer, Rect::from_size({2, 1}));
    renderer.present();

    assert(terminal.foreground(0, 0) == 5);
}

void test_split_divider_style() {
    Theme theme;

    StyleBox divider;
    divider.set_background_color(6);
    theme.set_style("split_container.divider", divider);

    SplitContainer split;
    apply_theme(split, theme);
    assert(split.get_divider_stylebox().get_background_color() == 6);
}

} // namespace

int main() {
    test_empty_theme_is_a_no_op();
    test_label_style_and_text_color();
    test_button_states();
    test_apply_walks_the_whole_tree();
    test_revision_and_presence();
    test_margins_affect_layout();
    test_manual_stylebox_wins_until_reapplied();
    test_spinbox_keeps_arrow_columns();
    test_progress_bar_fill_color();
    test_split_divider_style();

    std::cout << "theme: all checks passed." << std::endl;
    return 0;
}
