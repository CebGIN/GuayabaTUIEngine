#ifndef GUAYABA_NODES_UI_HBOX_HPP
#define GUAYABA_NODES_UI_HBOX_HPP

#include <string>

#include <guayaba/Core/Rect.hpp>
#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/UI/Container.hpp>

namespace guayaba {

/*
    A Container that lays its children out in a row, left to right.

    Each child gets a cell as wide as its own minimum size (set through
    set_custom_minimum_size); the gap between cells is `separation`. The cross
    axis fills: every cell spans the full height of the HBox's resolved rect, so
    a child anchored TopLeft sits at the HBox's top edge rather than at the
    strip's. How a child fills its cell is up to its own anchors and Grow.

    content_alignment places the strip inside the HBox: Begin at the left, Center
    (the default) in the middle, End at the right. If the HBox is wider than the
    strip and at least one child has Grow::Both horizontally, the leftover is
    split equally between those children (remainder to the first), and the strip
    spans the whole box.
*/
class HBox : public Container {
    int separation = 0;

public:
    explicit HBox(const std::string& node_name = "Unnamed HBox",
                  Vec2 node_position = {0, 0});

    void on_update_layout(const Rect& parent_rect) override;

    Vec2 get_minimum_size() const override;

    // --- Separation ---------------------------------------------------------

    int get_separation() const { return separation; }

    // Floored at zero.
    void set_separation(int new_separation) {
        separation = new_separation < 0 ? 0 : new_separation;
    }
};

} // namespace guayaba

#endif // GUAYABA_NODES_UI_HBOX_HPP