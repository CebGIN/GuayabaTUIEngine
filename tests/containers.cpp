// Headless layout checks for the containers added in Fase 1: GridContainer and
// MarginContainer. No terminal: a scene is built in memory and each child's
// resolved rect is asserted against the cell the container should have given it.

#include <cassert>
#include <iostream>
#include <memory>
#include <string>

#include <guayaba/Guayaba.hpp>

using namespace guayaba;

namespace {

constexpr Vec2 SCREEN{20, 10};

// A Control child whose minimum is its only size, so a cell can be read off the
// resolved rect without the child growing into it.
Control* add_cell(Node& parent, const std::string& name, Vec2 minimum) {
    NodeRef<Control> child = parent.add_child<Control>(name);
    child->set_custom_minimum_size(minimum);
    return child.get();
}

// --- GridContainer ----------------------------------------------------------

void test_grid_one_column() {
    auto grid = std::make_unique<GridContainer>("grid");
    grid->set_anchor_preset(AnchorPreset::FullRect);
    grid->set_content_alignment(ContentAlignment::Begin);
    grid->set_columns(1);

    Control* a = add_cell(*grid, "a", {4, 1});
    Control* b = add_cell(*grid, "b", {6, 2});
    Control* c = add_cell(*grid, "c", {5, 3});

    grid->update_layout(Rect::from_size(SCREEN));

    // One column: the widest minimum wins, and rows stack by their own height.
    assert((a->get_rect().pos == Vec2{0, 0}));
    assert((b->get_rect().pos == Vec2{0, 1}));
    assert((c->get_rect().pos == Vec2{0, 3}));
    assert((grid->get_minimum_size() == Vec2{6, 6}));
}

void test_grid_two_columns_and_minimums() {
    auto grid = std::make_unique<GridContainer>("grid");
    grid->set_anchor_preset(AnchorPreset::FullRect);
    grid->set_content_alignment(ContentAlignment::Begin);
    grid->set_columns(2);

    Control* a = add_cell(*grid, "a", {4, 1});  // row 0, col 0
    Control* b = add_cell(*grid, "b", {6, 2});  // row 0, col 1
    Control* c = add_cell(*grid, "c", {5, 3});  // row 1, col 0

    grid->update_layout(Rect::from_size(SCREEN));

    // Column 0 is as wide as its widest child (5), column 1 is 6; row 0 is 2
    // tall, row 1 is 3.
    assert((grid->get_minimum_size() == Vec2{11, 5}));
    assert((a->get_rect().pos == Vec2{0, 0}));
    assert((b->get_rect().pos == Vec2{5, 0}));
    assert((c->get_rect().pos == Vec2{0, 2}));
}

void test_grid_separations() {
    auto grid = std::make_unique<GridContainer>("grid");
    grid->set_anchor_preset(AnchorPreset::FullRect);
    grid->set_content_alignment(ContentAlignment::Begin);
    grid->set_columns(2);
    grid->set_h_separation(1);
    grid->set_v_separation(2);

    Control* a = add_cell(*grid, "a", {4, 1});
    Control* b = add_cell(*grid, "b", {6, 2});
    Control* c = add_cell(*grid, "c", {5, 3});

    grid->update_layout(Rect::from_size(SCREEN));

    assert((grid->get_minimum_size() == Vec2{12, 7}));
    assert((a->get_rect().pos == Vec2{0, 0}));
    assert((b->get_rect().pos == Vec2{6, 0}));
    assert((c->get_rect().pos == Vec2{0, 4}));
}

void test_grid_alignment() {
    auto grid = std::make_unique<GridContainer>("grid");
    grid->set_anchor_preset(AnchorPreset::FullRect);
    grid->set_columns(2);

    Control* a = add_cell(*grid, "a", {4, 1});
    Control* b = add_cell(*grid, "b", {6, 1});

    grid->update_layout(Rect::from_size(SCREEN));

    // Block 10x1 centred in 20x10: offset (5, 4).
    assert((a->get_rect().pos == Vec2{5, 4}));
    assert((b->get_rect().pos == Vec2{9, 4}));
}

// Two growers share the slack; the remainder goes to the first.
void test_grid_grow_splits_slack() {
    auto grid = std::make_unique<GridContainer>("grid");
    grid->set_anchor_preset(AnchorPreset::FullRect);
    grid->set_content_alignment(ContentAlignment::Begin);
    grid->set_columns(2);

    NodeRef<Control> a = grid->add_child<Control>("a");
    NodeRef<Control> b = grid->add_child<Control>("b");
    a->set_custom_minimum_size({4, 1});
    b->set_custom_minimum_size({6, 1});
    a->set_anchor_preset(AnchorPreset::FullRect);
    b->set_anchor_preset(AnchorPreset::FullRect);

    // 21 wide: block 10, slack 11, share 5 each, remainder 1 to column 0.
    grid->update_layout(Rect::from_size({21, 5}));

    assert(a->get_rect().pos.x == 0);
    assert(a->get_rect().size.x == 10);
    assert(b->get_rect().pos.x == 10);
    assert(b->get_rect().size.x == 11);
}

// A column with no Grow::Both child keeps its minimum, even when another column
// grows.
void test_grid_only_grower_columns_absorb_slack() {
    auto grid = std::make_unique<GridContainer>("grid");
    grid->set_anchor_preset(AnchorPreset::FullRect);
    grid->set_content_alignment(ContentAlignment::Begin);
    grid->set_columns(3);

    Control* a = add_cell(*grid, "a", {4, 1});
    NodeRef<Control> b = grid->add_child<Control>("b");
    Control* c = add_cell(*grid, "c", {2, 1});
    b->set_custom_minimum_size({6, 1});
    b->set_anchor_preset(AnchorPreset::FullRect);

    // Block 12, slack 8; only column 1 grows, to 14.
    grid->update_layout(Rect::from_size(SCREEN));

    assert(a->get_rect().pos.x == 0);
    assert(b->get_rect().pos.x == 4);
    assert(c->get_rect().pos.x == 18);
}

// A non-Control child is not part of the grid: it does not affect the minimum
// and is placed against the whole grid rect.
void test_grid_non_control_child() {
    auto grid = std::make_unique<GridContainer>("grid");
    grid->set_anchor_preset(AnchorPreset::FullRect);
    grid->set_columns(1);

    add_cell(*grid, "cell", {4, 1});
    NodeRef<Node2D> decoration = grid->add_child<Node2D>("decoration");

    grid->update_layout(Rect::from_size(SCREEN));

    assert((grid->get_minimum_size() == Vec2{4, 1}));
    // Block 4x1 centred in 20x10: offset (8, 4).
    assert((decoration->get_local_position() == Vec2{8, 4}));
}

// A hidden child takes no cell, no room and is not laid out.
void test_grid_hidden_child_is_skipped() {
    auto grid = std::make_unique<GridContainer>("grid");
    grid->set_anchor_preset(AnchorPreset::FullRect);
    grid->set_content_alignment(ContentAlignment::Begin);
    grid->set_columns(2);

    Control* a = add_cell(*grid, "a", {4, 1});
    Control* hidden = add_cell(*grid, "hidden", {9, 9});
    Control* b = add_cell(*grid, "b", {6, 2});

    hidden->set_visible(false);
    grid->update_layout(Rect::from_size(SCREEN));

    // Only a and b take part: a is (0,0), b moves up to row 0 column 1.
    assert((grid->get_minimum_size() == Vec2{10, 2}));
    assert((a->get_rect().pos == Vec2{0, 0}));
    assert((b->get_rect().pos == Vec2{4, 0}));
    assert(hidden->get_rect().size.x == 0);
}

} // namespace

int main() {
    test_grid_one_column();
    test_grid_two_columns_and_minimums();
    test_grid_separations();
    test_grid_alignment();
    test_grid_grow_splits_slack();
    test_grid_only_grower_columns_absorb_slack();
    test_grid_non_control_child();
    test_grid_hidden_child_is_skipped();

    std::cout << "containers: all checks passed." << std::endl;
    return 0;
}
