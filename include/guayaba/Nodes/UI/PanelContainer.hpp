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
    A Container that draws a StyleBox panel and stretches its child across the
    inside of it.

    The child gets everything inside the panel's border and margins, so Grow::Both
    or FullRect spans the panel exactly. The margin counts towards the minimum
    size, unlike a plain Container's padding.

    Only one child is expected; several are all stretched across the same content
    rect and overlap. Use a VBox or HBox inside the panel for more.
*/
class PanelContainer : public Container {
protected:
    // Protected so subclasses (ScrollView) can reuse the resolved box and content rect.
    StyleBox stylebox;

    // The area inside the border and margins: what the child gets. Empty when the panel is too small.
    Rect panel_content_rect() const;

public:
    explicit PanelContainer(const std::string& node_name = "Unnamed PanelContainer",
                            Vec2 node_position = {0, 0});

    void on_update_layout(const Rect& parent_rect) override;

    Vec2 get_minimum_size() const override;

    void on_draw(ConsoleRenderer& renderer, const Rect& clip) override;

    // --- Appearance ---------------------------------------------------------

    StyleBox& get_stylebox() { return stylebox; }
    const StyleBox& get_stylebox() const { return stylebox; }

    void set_stylebox(StyleBox new_stylebox) { stylebox = std::move(new_stylebox); }
};

} // namespace guayaba

#endif // GUAYABA_NODES_UI_PANELCONTAINER_HPP