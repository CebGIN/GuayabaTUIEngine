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

    const Rect content = content_rect();

    // The cursor walks the strip left to right. A child with a minimum to report
    // gets a cell exactly that wide and pushes the cursor along by it. A child
    // with none - a NodeTextMatrix, say - is handed the whole strip as its cell,
    // so it lands centred in the row like everything else, but it consumes no
    // width and no gap: there is no honest width to reserve for it, and
    // pretending otherwise would shove every later cell off the end of the strip.
    //
    // The gap is added after a cell that took up room rather than before the next
    // one, so a cell that took up none does not leave a gap behind it and split
    // the row into a wider space than the strip actually is.
    int x = content.pos.x;
    for (size_t i = 0; i < children.size(); ++i) {
        const Control* as_control = dynamic_cast<const Control*>(children[i].get());

        int cell_width = content.size.x;
        int advance = 0;
        if (as_control != nullptr) {
            cell_width = as_control->get_minimum_size().x;
            advance = cell_width;
        }

        place_child(i, Rect{Vec2{x, content.pos.y}, Vec2{cell_width, content.size.y}});

        x += advance;
        if (advance > 0) x += separation;
    }
}

} // namespace guayaba