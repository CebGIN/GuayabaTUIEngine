#ifndef GUAYABA_NODES_UI_VBOX_HPP
#define GUAYABA_NODES_UI_VBOX_HPP

#include <string>

#include <guayaba/Core/Rect.hpp>
#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/UI/Container.hpp>

namespace guayaba {

/*
    A Container that lays its children out in a column, top to bottom.

    The transpose of HBox: each child gets a cell as tall as its own minimum size,
    and the cross axis fills the full width of the VBox's resolved rect.
    content_alignment places the column inside the VBox: Begin at the top, Center
    (the default) in the middle, End at the bottom. If the VBox is taller than
    the column and at least one child has Grow::Both vertically, the leftover is
    split equally between those children (remainder to the first), and the column
    spans the whole box.
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

    // Floored at zero.
    void set_separation(int new_separation) {
        separation = new_separation < 0 ? 0 : new_separation;
    }
};

} // namespace guayaba

#endif // GUAYABA_NODES_UI_VBOX_HPP