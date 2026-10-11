#include <algorithm>

#include <guayaba/Nodes/UI/VBox.hpp>

namespace guayaba {

VBox::VBox(const std::string& node_name, Vec2 node_position)
    : Container(node_name, node_position) {}

Vec2 VBox::get_minimum_size() const {
    // Heights add up, widths compete: the column is as tall as every cell and
    // its gap together, and as wide as the widest child. Container's own minimum
    // still applies, so a VBox forced taller than its contents pads the column
    // rather than squeezing a cell.
    int width = 0;
    int height = 0;
    int cells = 0;

    for (const std::unique_ptr<Node>& child : children) {
        const Control* as_control = dynamic_cast<const Control*>(child.get());
        if (as_control == nullptr) continue;

        const Vec2 child_minimum = as_control->get_minimum_size();
        width = std::max(width, child_minimum.x);
        height += child_minimum.y;
        ++cells;
    }

    // The gap goes between two cells, never around the column, so an empty or
    // single-cell box costs nothing extra.
    if (cells > 1) height += separation * (cells - 1);

    const Vec2 own = Control::get_minimum_size();
    return {std::max(own.x, width), std::max(own.y, height)};
}

void VBox::update_layout(const Rect& parent_rect) {
    resolve_rect(parent_rect);

    const Rect content = content_rect();

    // The cursor walks the column downwards. A child with a minimum to report
    // gets a cell exactly that tall and pushes the cursor along by it. A child
    // with none - a NodeTextMatrix, say - is handed the whole column as its cell,
    // so it lands centred in the column like everything else, but it consumes no
    // height and no gap: there is no honest height to reserve for it, and
    // pretending otherwise would shove every later cell off the end of the column.
    //
    // The gap is added after a cell that took up room rather than before the next
    // one, so a cell that took up none does not leave a gap behind it and split
    // the column into a taller space than the strip actually is.
    int y = content.pos.y;
    for (size_t i = 0; i < children.size(); ++i) {
        const Control* as_control = dynamic_cast<const Control*>(children[i].get());

        int cell_height = content.size.y;
        int advance = 0;
        if (as_control != nullptr) {
            cell_height = as_control->get_minimum_size().y;
            advance = cell_height;
        }

        place_child(i, Rect{Vec2{content.pos.x, y}, Vec2{content.size.x, cell_height}});

        y += advance;
        if (advance > 0) y += separation;
    }
}

} // namespace guayaba