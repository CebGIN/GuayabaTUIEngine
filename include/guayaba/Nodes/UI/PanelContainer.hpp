#ifndef GUAYABA_NODES_UI_PANELCONTAINER_HPP
#define GUAYABA_NODES_UI_PANELCONTAINER_HPP

#include <string>

#include <guayaba/Core/Rect.hpp>
#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/UI/Container.hpp>
#include <guayaba/Renderer/ConsoleRenderer.hpp>
#include <guayaba/Resources/Stylebox.hpp>

namespace guayaba {

/*
    A Container that draws a panel and stretches its child across it.

    The one Container that fills rather than centres. Where a Container gives its
    child a block the size of the child's own minimum and leaves the rest as
    padding, a PanelContainer hands the child everything inside the panel's
    border and margins:

        +---------------------------+
        | +-----------------------+ |
        | |                       | |
        | |         child         | |  <- child fills the content rect
        | |                       | |
        | +-----------------------+ |
        +---------------------------+
          ^ StyleBox: background, border and margins

    The StyleBox is the same one Label draws, so the panel takes a background
    colour, a border and a content margin the same way. The margin is the gap
    between the panel's edge and its child, and - unlike a Container's padding -
    it counts towards the minimum size, because a PanelContainer forced to a
    smaller size than its margin should shrink its child, not its own frame.

    Only one child is expected. A PanelContainer with several children stretches
    all of them across the same content rect, which overlaps them; the last one
    drawn wins the pixels. Use a VBox or HBox inside the panel for more.

    As with every Container, the panel owns the child's position. A PanelContainer
    moves its child to the content rect and the child then fills it, so a child
    set to Grow::Both or the FullRect preset spans the panel exactly.
*/
class PanelContainer : public Container {
protected:
    // Protected rather than private so a subclass can build on the panel instead
    // of re-deriving it: ScrollView is a PanelContainer whose child is offset
    // and whose track sits outside this same frame, and both need the content
    // rect and the box the panel already resolved.
    StyleBox stylebox;

    // The area inside the border and the margins - what the child gets, and what
    // the minimum size has to make room for. Empty when the panel is too small
    // to hold anything, which the layout reads as "give the child nothing".
    Rect panel_content_rect() const;

public:
    explicit PanelContainer(const std::string& node_name = "Unnamed PanelContainer",
                            Vec2 node_position = {0, 0});

    void update_layout(const Rect& parent_rect) override;

    Vec2 get_minimum_size() const override;

    void draw(ConsoleRenderer& renderer, const Rect& clip) override;

    // --- Appearance ---------------------------------------------------------

    StyleBox& get_stylebox() { return stylebox; }
    const StyleBox& get_stylebox() const { return stylebox; }

    void set_stylebox(StyleBox new_stylebox) { stylebox = std::move(new_stylebox); }
};

} // namespace guayaba

#endif // GUAYABA_NODES_UI_PANELCONTAINER_HPP