#ifndef GUAYABA_NODES_UI_HBOX_HPP
#define GUAYABA_NODES_UI_HBOX_HPP

#include <string>

#include <guayaba/Core/Rect.hpp>
#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/UI/Container.hpp>

namespace guayaba {

/*
    A Container that lays its children out in a row, left to right.

    Where a Container piles its children on top of each other in the middle, an
    HBox gives each one a cell of its own along the horizontal axis and hands
    every child the full height of the strip:

        +---------------------------+
        |  [a]  [b]    [c]          |
        +---------------------------+
        |          ^ the HBox itself |
        +---------------------------+

    A cell is exactly as wide as the child's own minimum size, so a child that
    wants more room says so through set_custom_minimum_size. The gap between two
    cells is the separation; there is none before the first or after the last.

    The strip is as tall as the tallest child, and the whole strip is then
    centred inside the HBox by Container - the same rule a plain Container
    applies to its block.

    The child still owns its own placement inside its cell. An HBox gives it the
    cell and moves it there; how the child fills that cell is down to its own
    anchors and Grow, so a child that wants to be as tall as the strip sets
    Grow::Both or the FullRect preset.
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