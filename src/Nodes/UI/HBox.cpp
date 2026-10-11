#include <algorithm>

#include <guayaba/Nodes/UI/HBox.hpp>

namespace guayaba {

HBox::HBox(const std::string& node_name, Vec2 node_position)
    : Container(node_name, node_position) {}

Vec2 HBox::get_minimum_size() const {
    // Widths add up, heights compete: the strip is as wide as every cell and
    // its gap together, and as tall as the tallest child. Container's own
    // minimum still applies, so an HBox forced wider than its contents pads the
    // row rather than squeezing a cell.
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

    // The gap goes between two cells, never around the row, so an empty or
    // single-cell box costs nothing extra.
    if (cells > 1) width += separation * (cells - 1);

    const Vec2 own = Control::get_minimum_size();
    return {std::max(own.x, width), std::max(own.y, height)};
}

void HBox::update_layout(const Rect& parent_rect) {
    resolve_rect(parent_rect);

    const Rect box = get_rect();

    // The block: the strip's minimum size, already offset along the horizontal
    // axis by content_alignment. Only that axis is taken from it - the cells
    // below span the box's full height, not the block's.
    const Rect block = content_rect();

    // The leftover beyond the block, split between the children that asked to
    // grow on this axis. Grow::Both already means "stretch to fill the cell",
    // so a grown cell asks nothing new of the child: its own resolve_rect picks
    // up the bigger cell. With no expanders the shares stay zero and the
    // leftover simply remains empty, implied by the alignment.
    //
    // The resolved size is floored at get_minimum_size(), which is also what
    // block.size.x is, so the slack cannot go negative; the max is there
    // anyway because nothing downstream may be handed a negative extent.
    const int slack = std::max(0, box.size.x - block.size.x);
    int expanders = 0;
    for (const std::unique_ptr<Node>& child : children) {
        const Control* as_control = dynamic_cast<const Control*>(child.get());
        if (as_control != nullptr && as_control->get_grow_x() == Grow::Both) ++expanders;
    }
    const int share = expanders > 0 ? slack / expanders : 0;
    const int remainder = expanders > 0 ? slack % expanders : 0;

    // The shares above hand the expanders the whole leftover, so the strip
    // they grow ends at the box's own end edge. The offset content_rect()
    // applied was measured against the smaller, unexpanded block, and adding
    // it on top of that growth would push the last cell past the right edge -
    // so with expanders the walk starts at the box's own origin, which is the
    // same offset recomputed against the expanded block (the box, so zero).
    // Without them the aligned block stands as content_rect() left it.
    const int column_start = expanders > 0 ? box.pos.x : block.pos.x;

    // The cursor walks the strip left to right. A child with a minimum to
    // report gets a cell exactly that wide - grown first if it is one of the
    // expanders above - and pushes the cursor along by it. A child with none -
    // a NodeTextMatrix, say - is handed the whole block as its cell, so it
    // lands where the strip is, but it consumes no width and no gap: there is
    // no honest width to reserve for it, and pretending otherwise would shove
    // every later cell off the end of the strip.
    //
    // The gap is added after a cell that took up room rather than before the next
    // one, so a cell that took up none does not leave a gap behind it and split
    // the row into a wider space than the strip actually is.
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