#ifndef GUAYABA_NODES_UI_VBOX_HPP
#define GUAYABA_NODES_UI_VBOX_HPP

#include <string>

#include <guayaba/Core/Rect.hpp>
#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/UI/Container.hpp>

namespace guayaba {

/*
    A Container that lays its children out in a column, top to bottom.

    The transpose of HBox: each child gets a cell of its own along the vertical
    axis. The cross axis fills - every cell spans the full width of the VBox's
    resolved rect rather than the column's minimum width - so a child that
    stretches (Grow::Both or the FullRect preset) reaches both edges, and one
    anchored TopLeft sits at the VBox's left edge rather than at the column's.

        +-----------------+
        |  [a]            |
        |  [b]            |  <- each cell is the child's own minimum height
        |  [c]            |     and the full width of the VBox
        +-----------------+

    The column of cells is as tall as the children's minimums plus their
    separations, and content_alignment says where that column sits inside the
    VBox: at the top (Begin), in the middle (Center, the default) or at the
    bottom (End). Space the column does not use stays empty, implied by the
    alignment.

    The space is not always meant to stay empty. If the VBox is taller than the
    column and at least one child has Grow::Both on the vertical axis, the
    leftover is split between those children instead: each of their cells grows
    by an equal share of it, and the remainder goes to the first of them in
    order. The expanded column therefore spans the whole VBox - it is the box,
    not the minimum - so content_alignment has nothing left to offset and a
    box with expanders on the vertical axis lays out edge to edge whatever the
    alignment says. Grow::Both already means "stretch to fill the cell", so a
    grown cell asks nothing new of the child - a Label with fit_content and
    Grow::Both becomes a full-height filler whose text stays at its content's
    top-left.

    The child still owns its own placement inside its cell: the VBox decides
    how tall the cell is, and the child's own anchors and Grow decide what it
    does with the cell it was given.
*/
class VBox : public Container {
    int separation = 0;

public:
    explicit VBox(const std::string& node_name = "Unnamed VBox",
                  Vec2 node_position = {0, 0});

    void update_layout(const Rect& parent_rect) override;

    Vec2 get_minimum_size() const override;

    // --- Separation ---------------------------------------------------------

    int get_separation() const { return separation; }

    // Floored at zero: a negative gap would pull two cells into each other,
    // which is the one thing a column cannot mean.
    void set_separation(int new_separation) {
        separation = new_separation < 0 ? 0 : new_separation;
    }
};

} // namespace guayaba

#endif // GUAYABA_NODES_UI_VBOX_HPP