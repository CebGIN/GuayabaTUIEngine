// Headless layout tests for content alignment and cell expansion.
//
// No terminal and no draw(): a scene is built in memory, laid out against a
// 80x24 root rect, and each child's resolved rect is asserted against the cell
// the box should have handed it. Built only when GUAYABA_BUILD_TESTS is ON.

#include <cassert>
#include <cstdio>
#include <memory>

#include <guayaba/Core/NodeRef.hpp>
#include <guayaba/Core/Rect.hpp>
#include <guayaba/Nodes/UI/Container.hpp>
#include <guayaba/Nodes/UI/Control.hpp>
#include <guayaba/Nodes/UI/HBox.hpp>
#include <guayaba/Nodes/UI/VBox.hpp>

namespace {

using namespace guayaba;

constexpr Vec2 SCREEN{80, 24};

// A VBox child that hugs its cell on the main axis - keeps its own minimum
// height - and stretches across the box on the cross axis. Its resolved rect
// therefore shows where the cell starts and how wide the box made it, without
// the child itself growing into the leftover height.
Control* add_vbox_row(Node& parent, Vec2 minimum) {
    NodeRef<Control> child = parent.add_child<Control>();
    child->set_custom_minimum_size(minimum);
    child->set_anchor_right(1.0f);
    child->set_grow_x(Grow::Both);
    return child.get();
}

// The HBox mirror: hugs its own minimum width, stretches across the box's
// full height.
Control* add_hbox_column(Node& parent, Vec2 minimum) {
    NodeRef<Control> child = parent.add_child<Control>();
    child->set_custom_minimum_size(minimum);
    child->set_anchor_bottom(1.0f);
    child->set_grow_y(Grow::Both);
    return child.get();
}

// A child that fills its cell outright: FullRect spans the cell on both axes
// and Grow::Both stretches into it, so the resolved rect is the cell itself.
// On a VBox/HBox this is also an expander, since FullRect sets Grow::Both on
// both axes.
Control* add_filler_child(Node& parent, Vec2 minimum) {
    NodeRef<Control> child = parent.add_child<Control>();
    child->set_custom_minimum_size(minimum);
    child->set_anchor_preset(AnchorPreset::FullRect);
    return child.get();
}

// A plain Container hands every child the same rect: its own rect shrunk to
// its minimum and offset by content_alignment. The filler resolves to exactly
// that rect, so its resolved position is the alignment's offset.
void test_plain_container_alignment() {
    const struct {
        ContentAlignment alignment;
        Vec2 expected_pos;
    } cases[] = {
        {ContentAlignment::Center, {35, 10}},  // (80-10)/2, (24-4)/2
        {ContentAlignment::Begin, {0, 0}},
        {ContentAlignment::End, {70, 20}},    // 80-10, 24-4
    };

    for (const auto& test : cases) {
        std::unique_ptr<Container> root = std::make_unique<Container>();
        root->set_anchor_preset(AnchorPreset::FullRect);
        root->set_content_alignment(test.alignment);
        Control* child = add_filler_child(*root, {10, 4});

        root->update_layout(Rect::from_size(SCREEN));

        assert((child->get_rect() == Rect{test.expected_pos, Vec2{10, 4}}));
    }
}

// The default is Center: the column of minimum-height cells sits in the middle
// of the box, but every cell is already as wide as the box - the cross axis
// fills, it is not shrunk to the column's minimum width.
void test_vbox_center_default() {
    std::unique_ptr<VBox> root = std::make_unique<VBox>();
    root->set_anchor_preset(AnchorPreset::FullRect);
    Control* a = add_vbox_row(*root, {10, 3});
    Control* b = add_vbox_row(*root, {10, 3});

    root->update_layout(Rect::from_size(SCREEN));

    assert(root->get_content_alignment() == ContentAlignment::Center);
    assert((a->get_rect() == Rect{Vec2{0, 9}, Vec2{80, 3}}));
    assert((b->get_rect() == Rect{Vec2{0, 12}, Vec2{80, 3}}));
}

// Begin: the first child's cell starts at the top-left of the box.
void test_vbox_begin() {
    std::unique_ptr<VBox> root = std::make_unique<VBox>();
    root->set_anchor_preset(AnchorPreset::FullRect);
    root->set_content_alignment(ContentAlignment::Begin);
    Control* a = add_vbox_row(*root, {10, 3});
    Control* b = add_vbox_row(*root, {10, 3});

    root->update_layout(Rect::from_size(SCREEN));

    assert((a->get_rect() == Rect{Vec2{0, 0}, Vec2{80, 3}}));
    assert((b->get_rect() == Rect{Vec2{0, 3}, Vec2{80, 3}}));
}

// End: the last child's cell ends at the bottom edge of the box.
void test_vbox_end() {
    std::unique_ptr<VBox> root = std::make_unique<VBox>();
    root->set_anchor_preset(AnchorPreset::FullRect);
    root->set_content_alignment(ContentAlignment::End);
    add_vbox_row(*root, {10, 3});
    Control* b = add_vbox_row(*root, {10, 3});

    root->update_layout(Rect::from_size(SCREEN));

    assert((b->get_rect() == Rect{Vec2{0, 21}, Vec2{80, 3}}));
    assert(b->get_rect().bottom() == SCREEN.y);
}

// One Grow::Both child absorbs all the slack: the column starts at the top
// (Begin) and the expander's cell runs to the bottom edge.
void test_vbox_single_expander() {
    std::unique_ptr<VBox> root = std::make_unique<VBox>();
    root->set_anchor_preset(AnchorPreset::FullRect);
    root->set_content_alignment(ContentAlignment::Begin);
    Control* a = add_vbox_row(*root, {10, 3});
    Control* e = add_filler_child(*root, {10, 4});

    root->update_layout(Rect::from_size(SCREEN));

    // Slack = 24 - (3 + 4) = 17, all of it on the expander: 4 + 17 = 21.
    assert((a->get_rect() == Rect{Vec2{0, 0}, Vec2{80, 3}}));
    assert((e->get_rect() == Rect{Vec2{0, 3}, Vec2{80, 21}}));
    assert(e->get_rect().bottom() == SCREEN.y);
}

// Two Grow::Both children split the slack; the remainder goes to the first.
// With a separation of 1 the column is 3 + 1 + 4 + 1 + 4 = 13 tall, so the
// slack is 11: shares of 5 each, one extra cell on the first expander.
void test_vbox_two_expanders_split_slack() {
    std::unique_ptr<VBox> root = std::make_unique<VBox>();
    root->set_anchor_preset(AnchorPreset::FullRect);
    root->set_content_alignment(ContentAlignment::Begin);
    root->set_separation(1);
    Control* a = add_vbox_row(*root, {10, 3});
    Control* e1 = add_filler_child(*root, {10, 4});
    Control* e2 = add_filler_child(*root, {10, 4});

    root->update_layout(Rect::from_size(SCREEN));

    assert((a->get_rect() == Rect{Vec2{0, 0}, Vec2{80, 3}}));
    assert((e1->get_rect() == Rect{Vec2{0, 4}, Vec2{80, 10}}));   // 4 + 5 + 1
    assert((e2->get_rect() == Rect{Vec2{0, 15}, Vec2{80, 9}}));   // 4 + 5
    assert(e2->get_rect().bottom() == SCREEN.y);
}

// Center and End used to add their alignment offset on top of the expanded
// column, so the expander ran past the bottom edge by that offset. The
// leftover the alignment would have left empty is exactly the slack the
// expanders take, so the expanded column is the whole box: it runs edge to
// edge and the offset against it is zero.
void test_vbox_expander_alignment_no_overshoot() {
    const ContentAlignment alignments[] = {ContentAlignment::Center,
                                           ContentAlignment::End};
    for (const ContentAlignment alignment : alignments) {
        std::unique_ptr<VBox> root = std::make_unique<VBox>();
        root->set_anchor_preset(AnchorPreset::FullRect);
        root->set_content_alignment(alignment);
        Control* a = add_vbox_row(*root, {10, 3});
        Control* e = add_filler_child(*root, {10, 4});

        root->update_layout(Rect::from_size(SCREEN));

        // Slack = 24 - (3 + 4) = 17, all of it on the expander: 4 + 17 = 21.
        assert((a->get_rect() == Rect{Vec2{0, 0}, Vec2{80, 3}}));
        assert((e->get_rect() == Rect{Vec2{0, 3}, Vec2{80, 21}}));
        assert(e->get_rect().bottom() == SCREEN.y);
    }
}

// The HBox mirror of test_vbox_center_default: the row is centred on the main
// axis and every cell is as tall as the box.
void test_hbox_center_default() {
    std::unique_ptr<HBox> root = std::make_unique<HBox>();
    root->set_anchor_preset(AnchorPreset::FullRect);
    Control* a = add_hbox_column(*root, {4, 5});
    Control* b = add_hbox_column(*root, {4, 5});

    root->update_layout(Rect::from_size(SCREEN));

    assert(root->get_content_alignment() == ContentAlignment::Center);
    assert((a->get_rect() == Rect{Vec2{36, 0}, Vec2{4, 24}}));
    assert((b->get_rect() == Rect{Vec2{40, 0}, Vec2{4, 24}}));
}

// Begin: the first child's cell starts at the top-left of the box.
void test_hbox_begin() {
    std::unique_ptr<HBox> root = std::make_unique<HBox>();
    root->set_anchor_preset(AnchorPreset::FullRect);
    root->set_content_alignment(ContentAlignment::Begin);
    Control* a = add_hbox_column(*root, {4, 5});
    Control* b = add_hbox_column(*root, {4, 5});

    root->update_layout(Rect::from_size(SCREEN));

    assert((a->get_rect() == Rect{Vec2{0, 0}, Vec2{4, 24}}));
    assert((b->get_rect() == Rect{Vec2{4, 0}, Vec2{4, 24}}));
}

// End: the last child's cell ends at the right edge of the box.
void test_hbox_end() {
    std::unique_ptr<HBox> root = std::make_unique<HBox>();
    root->set_anchor_preset(AnchorPreset::FullRect);
    root->set_content_alignment(ContentAlignment::End);
    add_hbox_column(*root, {4, 5});
    Control* b = add_hbox_column(*root, {4, 5});

    root->update_layout(Rect::from_size(SCREEN));

    assert((b->get_rect() == Rect{Vec2{76, 0}, Vec2{4, 24}}));
    assert(b->get_rect().right() == SCREEN.x);
}

// One Grow::Both child absorbs all the slack on the horizontal axis.
void test_hbox_single_expander() {
    std::unique_ptr<HBox> root = std::make_unique<HBox>();
    root->set_anchor_preset(AnchorPreset::FullRect);
    root->set_content_alignment(ContentAlignment::Begin);
    Control* a = add_hbox_column(*root, {4, 5});
    Control* e = add_filler_child(*root, {4, 5});

    root->update_layout(Rect::from_size(SCREEN));

    // Slack = 80 - (4 + 4) = 72, all of it on the expander: 4 + 72 = 76.
    assert((a->get_rect() == Rect{Vec2{0, 0}, Vec2{4, 24}}));
    assert((e->get_rect() == Rect{Vec2{4, 0}, Vec2{76, 24}}));
    assert(e->get_rect().right() == SCREEN.x);
}

// Two Grow::Both children split the slack; the remainder goes to the first.
// The row is 4 + 1 + 6 + 1 + 5 = 17 wide, so the slack is 63: shares of 31
// each, one extra cell on the first expander.
void test_hbox_two_expanders_split_slack() {
    std::unique_ptr<HBox> root = std::make_unique<HBox>();
    root->set_anchor_preset(AnchorPreset::FullRect);
    root->set_content_alignment(ContentAlignment::Begin);
    root->set_separation(1);
    Control* a = add_hbox_column(*root, {4, 5});
    Control* e1 = add_filler_child(*root, {6, 5});
    Control* e2 = add_filler_child(*root, {5, 5});

    root->update_layout(Rect::from_size(SCREEN));

    assert((a->get_rect() == Rect{Vec2{0, 0}, Vec2{4, 24}}));
    assert((e1->get_rect() == Rect{Vec2{5, 0}, Vec2{38, 24}}));   // 6 + 31 + 1
    assert((e2->get_rect() == Rect{Vec2{44, 0}, Vec2{36, 24}}));  // 5 + 31
    assert(e2->get_rect().right() == SCREEN.x);
}

// The HBox mirror of test_vbox_expander_alignment_no_overshoot: Center and End
// used to push the expanded cell past the right edge by the alignment offset.
void test_hbox_expander_alignment_no_overshoot() {
    const ContentAlignment alignments[] = {ContentAlignment::Center,
                                           ContentAlignment::End};
    for (const ContentAlignment alignment : alignments) {
        std::unique_ptr<HBox> root = std::make_unique<HBox>();
        root->set_anchor_preset(AnchorPreset::FullRect);
        root->set_content_alignment(alignment);
        Control* a = add_hbox_column(*root, {4, 5});
        Control* e = add_filler_child(*root, {4, 5});

        root->update_layout(Rect::from_size(SCREEN));

        // Slack = 80 - (4 + 4) = 72, all of it on the expander: 4 + 72 = 76.
        assert((a->get_rect() == Rect{Vec2{0, 0}, Vec2{4, 24}}));
        assert((e->get_rect() == Rect{Vec2{4, 0}, Vec2{76, 24}}));
        assert(e->get_rect().right() == SCREEN.x);
    }
}

} // namespace

int main() {
    test_plain_container_alignment();
    test_vbox_center_default();
    test_vbox_begin();
    test_vbox_end();
    test_vbox_single_expander();
    test_vbox_two_expanders_split_slack();
    test_vbox_expander_alignment_no_overshoot();
    test_hbox_center_default();
    test_hbox_begin();
    test_hbox_end();
    test_hbox_single_expander();
    test_hbox_two_expanders_split_slack();
    test_hbox_expander_alignment_no_overshoot();

    std::puts("layout_alignment: all checks passed.");
    return 0;
}
