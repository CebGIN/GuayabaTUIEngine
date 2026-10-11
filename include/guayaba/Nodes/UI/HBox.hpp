#ifndef GUAYABA_NODES_UI_HBOX_HPP
#define GUAYABA_NODES_UI_HBOX_HPP

#include <string>

#include <guayaba/Core/Rect.hpp>
#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/UI/Container.hpp>

namespace guayaba {

/*
    A Container that lays its children out in a row, left to right.

    Where a Container piles its children on top of each other inside one
    aligned block, an HBox gives each one a cell of its own along the
    horizontal axis. The cross axis fills - every cell spans the full height of
    the HBox's resolved rect rather than the strip's minimum height - so a
    child that stretches (Grow::Both or the FullRect preset) reaches both
    edges, and one anchored TopLeft sits at the HBox's top edge rather than at
    the strip's.

        +---------------------------+
        |  [a]  [b]    [c]          |  <- each cell is the child's own
        +---------------------------+     minimum width and the full height
        |          ^ the HBox itself |
        +---------------------------+

    A cell is exactly as wide as the child's own minimum size, so a child that
    wants more room says so through set_custom_minimum_size. The gap between two
    cells is the separation; there is none before the first or after the last.

    The strip of cells is as wide as the children's minimums plus their
    separations, and content_alignment says where that strip sits inside the
    HBox: at the left (Begin), in the middle (Center, the default) or at the
    right (End). Space the strip does not use stays empty, implied by the
    alignment.

    The space is not always meant to stay empty. If the HBox is wider than the
    strip and at least one child has Grow::Both on the horizontal axis, the
    leftover is split between those children instead: each of their cells grows
    by an equal share of it, and the remainder goes to the first of them in
    order. Grow::Both already means "stretch to fill the cell", so a grown cell
    asks nothing new of the child.

    The child still owns its own placement inside its cell. An HBox gives it the
    cell and moves it there; how the child fills that cell is down to its own
    anchors and Grow.
*/
class HBox : public Container {
    int separation = 0;

public:
    explicit HBox(const std::string& node_name = "Unnamed HBox",
                  Vec2 node_position = {0, 0});

    void update_layout(const Rect& parent_rect) override;

    Vec2 get_minimum_size() const override;

    // --- Separation ---------------------------------------------------------

    int get_separation() const { return separation; }

    // Floored at zero, like every other extent here: a negative gap would pull
    // two cells into each other, which is the one thing a row cannot mean.
    void set_separation(int new_separation) {
        separation = new_separation < 0 ? 0 : new_separation;
    }
};

} // namespace guayaba

#endif // GUAYABA_NODES_UI_HBOX_HPP