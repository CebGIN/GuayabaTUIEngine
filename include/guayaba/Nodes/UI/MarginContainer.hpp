#ifndef GUAYABA_NODES_UI_MARGINCONTAINER_HPP
#define GUAYABA_NODES_UI_MARGINCONTAINER_HPP

#include <string>

#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/UI/Container.hpp>
#include <guayaba/Resources/Stylebox.hpp>

namespace guayaba {

/*
    A Container that insets its children by a margin on each side. The margin is
    taken off the resolved rect and the children are laid out against what is
    left, so the content follows the container's own anchors and size.

    Its minimum is the largest child minimum plus the margins; with no children it
    is the margins themselves. Negative insets are normalized to zero. It draws
    nothing.
*/
class MarginContainer : public Container {
private:
    EdgeSize margins;

    // The resolved rect minus the margins, never negative.
    Rect margin_content_rect() const;

public:
    explicit MarginContainer(const std::string& node_name = "Unnamed MarginContainer",
                             Vec2 node_position = {0, 0});

    void on_update_layout(const Rect& parent_rect) override;

    Vec2 get_minimum_size() const override;

    // --- Margins ------------------------------------------------------------

    // Each inset is floored at zero.
    void set_margins(EdgeSize new_margins);
    void set_margins(int left, int top, int right, int bottom);

    EdgeSize get_margins() const { return margins; }
};

} // namespace guayaba

#endif // GUAYABA_NODES_UI_MARGINCONTAINER_HPP
