#ifndef GUAYABA_NODES_UI_CONTAINER_HPP
#define GUAYABA_NODES_UI_CONTAINER_HPP

#include <string>

#include <guayaba/Core/Rect.hpp>
#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/UI/Control.hpp>

namespace guayaba {

/*
    A Control that arranges its children instead of drawing anything.

    It differs from a bare Control in two ways:

        get_minimum_size()   grows to fit the largest minimum size among its
                             Control children, so a Container pushed to a corner
                             still reserves room for what is inside it

        update_layout()      hands its children a rect that is already centred
                             inside the Container's own

    The centred rect is sized to the Container's own minimum size and then
    centred in the resolved rect, so the block of children sits in the middle
    with whatever padding the Container was given around it.

    Every child resolves against that same rect, and each one places itself
    inside it by its own anchors and Grow. Children therefore land wherever
    their own settings say and can overlap - a Container is not a stack, and
    nothing here rearranges siblings to avoid an overlap.

    A Container owns the positions of its children.

    That is the rule that makes the centring work, and it is the price of it. A
    child laid out against the centred rect would still draw at the Container's
    origin, because Control takes its position from the Node2D position chain and
    reads the parent rect for its size alone - so the shift has to travel down
    the positions. The Container therefore rewrites each child's local_position
    on every layout pass, and a child cannot rely on holding one:

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
    centred rect like anything else and simply does not reserve space for itself.
    It is still placed with everything else, though - every positioned child is.
*/
class Container : public Control {
protected:
    // The region the children resolve against: the Container's own rect shrunk
    // to its minimum size and centred inside it.
    //
    // Shrinking to the minimum size rather than to the resolved size is what
    // centres the children as a block. Handing down the full rect instead would
    // pin every child to the Container's top-left corner, since Grow::None reads
    // its alignment off the start anchor rather than off the rect's size.
    //
    // The offset is an integer division, so an odd leftover biases toward the
    // start edge - the same choice Control::update_layout makes when it centres
    // a control.
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
};

} // namespace guayaba

#endif // GUAYABA_NODES_UI_CONTAINER_HPP