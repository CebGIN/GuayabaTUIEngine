#include <algorithm>

#include <guayaba/Nodes/UI/HBox.hpp>

namespace guayaba {

HBox::HBox(const std::string& node_name, Vec2 node_position)
    : Container(node_name, node_position) {}

Vec2 HBox::get_minimum_size() const {
    // Widths add up plus the gaps; height is the tallest child. Control's own
    // minimum still floors it.
    int width = 0;
    int height = 0;
    int cells = 0;

    for (const std::unique_ptr<Node>& child : children) {
        const Control* as_control = dynamic_cast<const Control*>(child.get());
        if (as_control == nullptr) continue;

        const Vec2 child_minimum = as_control->get_minimum_size();
        width += child_minimum.x;
        height = std::max(height, child_minimum.y);
        ++cells;
    }

    // The gap goes between two cells, never around the row.
    if (cells > 1) width += separation * (cells - 1);

    const Vec2 own = Control::get_minimum_size();
    return {std::max(own.x, width), std::max(own.y, height)};
}

void HBox::update_layout(const Rect& parent_rect) {
    resolve_rect(parent_rect);

    const Rect box = get_rect();

    // The strip's minimum size, already offset horizontally by
    // content_alignment. The cells below span the box's full height.
    const Rect block = content_rect();

    // The leftover beyond the block, split between the children with Grow::Both on
    // this axis; with no expanders the leftover stays empty. The resolved size is
    // floored at the minimum, but the max guards against a negative extent.
    const int slack = std::max(0, box.size.x - block.size.x);
    int expanders = 0;
    for (const std::unique_ptr<Node>& child : children) {
        const Control* as_control = dynamic_cast<const Control*>(child.get());
        if (as_control != nullptr && as_control->get_grow_x() == Grow::Both) ++expanders;
    }
    const int share = expanders > 0 ? slack / expanders : 0;
    const int remainder = expanders > 0 ? slack % expanders : 0;

    // With expanders the strip fills the box, so the walk starts at the box's
    // origin rather than the offset, smaller block. Without them the aligned block
    // stands as content_rect() left it.
    const int column_start = expanders > 0 ? box.pos.x : block.pos.x;

    // The cursor walks left to right. A Control child gets a cell as wide as its
    // minimum (grown if it is an expander) and advances the cursor; a child with
    // no minimum gets the whole block and advances nothing. The gap is added after
    // a cell that took up room, so a zero-width cell leaves none behind.
    int x = column_start;
    int expanders_seen = 0;
    for (size_t i = 0; i < children.size(); ++i) {
        const Control* as_control = dynamic_cast<const Control*>(children[i].get());

        int cell_width = block.size.x;
        int advance = 0;
        if (as_control != nullptr) {
            cell_width = as_control->get_minimum_size().x;
            if (as_control->get_grow_x() == Grow::Both) {
                cell_width += share;
                if (expanders_seen < remainder) ++cell_width;
                ++expanders_seen;
            }
            advance = cell_width;
        }

        place_child(i, Rect{Vec2{x, box.pos.y}, Vec2{cell_width, box.size.y}});

        x += advance;
        if (advance > 0) x += separation;
    }
}

} // namespace guayaba