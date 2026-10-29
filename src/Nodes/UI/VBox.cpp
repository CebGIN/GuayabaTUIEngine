#include <algorithm>

#include <guayaba/Nodes/UI/VBox.hpp>

namespace guayaba {

VBox::VBox(const std::string& node_name, Vec2 node_position)
    : Container(node_name, node_position) {}

Vec2 VBox::get_minimum_size() const {
    // Heights add up plus the gaps; width is the widest child. Control's own
    // minimum still floors it.
    int width = 0;
    int height = 0;
    int cells = 0;

    for (const std::unique_ptr<Node>& child : children) {
        if (!child->is_visible()) continue;

        const Control* as_control = dynamic_cast<const Control*>(child.get());
        if (as_control == nullptr) continue;

        const Vec2 child_minimum = as_control->get_minimum_size();
        width = std::max(width, child_minimum.x);
        height += child_minimum.y;
        ++cells;
    }

    // The gap goes between two cells, never around the column.
    if (cells > 1) height += separation * (cells - 1);

    const Vec2 own = Control::get_minimum_size();
    return {std::max(own.x, width), std::max(own.y, height)};
}

void VBox::on_update_layout(const Rect& parent_rect) {
    resolve_rect(parent_rect);

    const Rect box = get_rect();

    // The column's minimum size, already offset vertically by content_alignment.
    // The cells below span the box's full width.
    const Rect block = content_rect();

    // The leftover beyond the block, split between the children with Grow::Both on
    // this axis; with no expanders the leftover stays empty. The resolved size is
    // floored at the minimum, but the max guards against a negative extent.
    const int slack = std::max(0, box.size.y - block.size.y);
    int expanders = 0;
    for (const std::unique_ptr<Node>& child : children) {
        if (!child->is_visible()) continue;

        const Control* as_control = dynamic_cast<const Control*>(child.get());
        if (as_control != nullptr && as_control->get_grow_y() == Grow::Both) ++expanders;
    }
    const int share = expanders > 0 ? slack / expanders : 0;
    const int remainder = expanders > 0 ? slack % expanders : 0;

    // With expanders the column fills the box, so the walk starts at the box's
    // origin rather than the offset, smaller block. Without them the aligned block
    // stands as content_rect() left it.
    const int column_start = expanders > 0 ? box.pos.y : block.pos.y;

    // The cursor walks downwards. A Control child gets a cell as tall as its
    // minimum (grown if it is an expander) and advances the cursor; a child with
    // no minimum gets the whole block and advances nothing. The gap is added after
    // a cell that took up room, so a zero-height cell leaves none behind.
    int y = column_start;
    int expanders_seen = 0;
    for (size_t i = 0; i < children.size(); ++i) {
        // A hidden child takes no cell and leaves no gap behind.
        if (!children[i]->is_visible()) continue;

        const Control* as_control = dynamic_cast<const Control*>(children[i].get());

        int cell_height = block.size.y;
        int advance = 0;
        if (as_control != nullptr) {
            cell_height = as_control->get_minimum_size().y;
            if (as_control->get_grow_y() == Grow::Both) {
                cell_height += share;
                if (expanders_seen < remainder) ++cell_height;
                ++expanders_seen;
            }
            advance = cell_height;
        }

        place_child(i, Rect{Vec2{box.pos.x, y}, Vec2{box.size.x, cell_height}});

        y += advance;
        if (advance > 0) y += separation;
    }
}

} // namespace guayaba