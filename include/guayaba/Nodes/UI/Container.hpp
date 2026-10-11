#ifndef GUAYABA_NODES_UI_CONTAINER_HPP
#define GUAYABA_NODES_UI_CONTAINER_HPP

#include <string>

#include <guayaba/Core/Rect.hpp>
#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/UI/Control.hpp>

namespace guayaba {

// Where a Container puts the block of children inside its own resolved rect,
// on both axes. Begin keeps the block at the start edge (top-left), End pushes
// it to the end edge (bottom-right), and Center - the default - splits the
// leftover space, biasing an odd leftover toward the start edge.
//
// VBox and HBox read it on the main axis alone; their cross axis always fills
// the resolved rect. See the class comments there.
enum class ContentAlignment {
    Begin,
    Center,
    End,
};

/*
    A Control that arranges its children instead of drawing anything.

    It differs from a bare Control in two ways:

        get_minimum_size()   grows to fit the largest minimum size among its
                             Control children, so a Container pushed to a corner
                             still reserves room for what is inside it

        update_layout()      hands its children a rect that is already placed
                             inside the Container's own by content_alignment

    That rect is sized to the Container's own minimum size and then offset
    inside the resolved rect by content_alignment, so the block of children
    stays a block with padding around it. Center, the default, puts the block
    in the middle on both axes - the placement every Container had before the
    alignment existed, and the one an existing scene keeps without asking.
    Begin puts the block at the top-left and End at the bottom-right.

    Every child resolves against that same rect, and each one places itself
    inside it by its own anchors and Grow. Children therefore land wherever
    their own settings say and can overlap - a Container is not a stack, and
    nothing here rearranges siblings to avoid an overlap.

    A Container owns the positions of its children.

    That is the rule that makes the placement work, and it is the price of it.
    A child laid out against the content rect would still draw at the
    Container's origin, because Control takes its position from the Node2D
    position chain and reads the parent rect for its size alone - so the shift
    has to travel down the positions. The Container therefore rewrites each
    child's local_position on every layout pass, and a child cannot rely on
    holding one:

container->add_child<Label>()      local_position is the Container's
                                           business, not the label's

    What the child keeps is where it sits inside that block. A child given
    Grow::Both fills its cell, one given FullRect fills it outright, and one given
    nothing anchors to its top-left - but the cell is the container's to choose
    and the child's own position never moves it out of the cell it was given.

    HBox, VBox and PanelContainer build on this. They give each child a different
    cell rather than moving them all by one shared offset.

    Only Control descendants contribute to the minimum size. A plain Node or a
    NodeTextMatrix has no minimum to report, so it is laid out against the
    content rect like anything else and simply does not reserve space for itself.
    It is still placed with everything else, though - every positioned child is.
*/
class Container : public Control {
private:
    // Where content_rect() puts the block of children inside the resolved
    // rect. Center is the default so that a Container that is not told
    // otherwise still places its children the way it always has.
    ContentAlignment content_alignment = ContentAlignment::Center;

protected:
    // The region the children resolve against: the Container's own rect shrunk
    // to its minimum size and offset inside it by content_alignment.
    //
    // Shrinking to the minimum size rather than to the resolved size is what
    // keeps the children a block with padding around them rather than pinning
    // every one of them to the Container's top-left corner, which is what
    // handing down the full rect would do - Grow::None reads its alignment off
    // the start anchor rather than off the rect's size.
    //
    // The offset is where the alignment shows: Begin at the start edge, End at
    // the end edge, and Center splitting the leftover with an integer
    // division, so an odd leftover biases toward the start edge - the same
    // choice Control::update_layout makes when it centres a control.
    Rect content_rect() const;

    // Hands `rect` to the child at `index` and puts it at that rect's top-left.
    //
    // The position is assigned outright, every pass, and never added to what was
    // there before. That is what makes it correct without any bookkeeping: there
    // is no accumulated shift to take back, so a child removed, reordered or
    // added cannot inherit anything from whichever child used to sit at its
    // index. Doing it the other way round - shift by a delta and remember the
    // delta - needs that memory kept per child and kept honest, and the whole
    // reason it is here is that the container has already decided where this
    // child goes.
    //
    // Both halves are needed together, because neither reaches a Control on its
    // own: the rect only reaches its size, since its position comes from the
    // Node2D chain.
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