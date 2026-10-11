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
    axis, every cell is as wide as the strip, and the whole column is then
    centred inside the VBox.

        +-----------------+
        |  [a]            |
        |  [b]            |  <- each cell is the child's own minimum height
        |  [c]            |
        +-----------------+

    The child still owns its own placement inside its cell, so one that wants to
    span the full width of the column sets Grow::Both or the FullRect preset.
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