#ifndef GUAYABA_NODES_UI_CONTAINER_HPP
#define GUAYABA_NODES_UI_CONTAINER_HPP

#include <string>

#include <guayaba/Core/Rect.hpp>
#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/UI/Control.hpp>

namespace guayaba {

// Where a Container puts the block of children inside its own resolved rect, on
// both axes. Begin at the start edge, End at the end edge, Center (the default)
// splits the leftover, biasing an odd leftover toward the start. VBox and HBox
// read it on the main axis only.
enum class ContentAlignment {
    Begin,
    Center,
    End,
};

/*
    A Control that arranges its children instead of drawing anything.

    get_minimum_size() grows to fit the largest minimum among its Control
    children. update_layout() hands its children a rect sized to that minimum and
    offset inside the resolved rect by content_alignment, so the block of children
    stays a block with padding around it.

    Every child resolves against that same rect and places itself inside it by its
    own anchors and Grow, so children can overlap. The Container rewrites each
    child's local_position on every layout pass: the child's cell is the
    container's to choose, and the child's own position never moves it out of it.

    HBox, VBox and PanelContainer build on this, giving each child a different cell.

    Only Control descendants contribute to the minimum size; a plain Node or
    NodeTextMatrix is placed but reserves no space.
*/
class Container : public Control {
private:
    // Where content_rect() puts the block of children inside the resolved rect.
    ContentAlignment content_alignment = ContentAlignment::Center;

protected:
    // The region the children resolve against: the Container's rect shrunk to its
    // minimum size and offset inside it by content_alignment.
    Rect content_rect() const;

    // Hands `rect` to the child at `index` and puts it at that rect's top-left. The
    // position is assigned outright every pass, never accumulated, so there is no
    // stale shift to take back. Both halves are needed: the rect reaches only the
    // child's size, since its position comes from the Node2D chain.
    void place_child(size_t index, const Rect& rect);

public:
    explicit Container(const std::string& node_name = "Unnamed Container",
                       Vec2 node_position = {0, 0});

    void update_layout(const Rect& parent_rect) override;

    Vec2 get_minimum_size() const override;

    // --- Alignment ----------------------------------------------------------

    ContentAlignment get_content_alignment() const { return content_alignment; }

    void set_content_alignment(ContentAlignment new_alignment) {
        content_alignment = new_alignment;
    }
};

} // namespace guayaba

#endif // GUAYABA_NODES_UI_CONTAINER_HPP