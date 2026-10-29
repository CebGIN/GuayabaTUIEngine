#ifndef GUAYABA_NODES_UI_SPLITCONTAINER_HPP
#define GUAYABA_NODES_UI_SPLITCONTAINER_HPP

#include <string>
#include <vector>

#include <guayaba/Core/InputEvent.hpp>
#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/UI/Container.hpp>
#include <guayaba/Resources/Stylebox.hpp>

namespace guayaba {

enum class SplitOrientation {
    VerticalDivider,    // two panels side by side
    HorizontalDivider,  // two panels stacked
};

/*
    Two panels with a draggable divider between them.

    The first two visible Control children are the panels; extra Control children
    and non-Control children are ignored. `split_ratio` is a 0..1 fraction of the
    free space, so the split survives a resize and the configured minimums are
    always respected:

        free  = max(0, total - divider_width - min_first - min_second)
        first = min_first + round(split_ratio * free)

    Dragging the divider captures the mouse, so motion outside the control still
    moves it. With focus, the arrow keys move one cell per press. The divider is
    painted only through its StyleBox, empty by default: it is invisible but still
    grabbable until a style is set.
*/
class SplitContainer : public Container {
private:
    SplitOrientation orientation = SplitOrientation::VerticalDivider;
    float split_ratio = 0.5f;

    int min_first = 0;
    int min_second = 0;
    int divider_width = 1;

    StyleBox divider_stylebox;

    bool dragging = false;
    int drag_axis = 0;

    // The first two visible Control children, in tree order.
    std::vector<Control*> panels() const;

    int axis_extent(const Rect& box) const;
    int axis_coord(Vec2 point) const;

    // total - divider_width - min_first - min_second, never negative.
    int free_extent() const;

    // The first panel's extent in cells, clamped to what the box can hold.
    int first_extent() const;

    Rect divider_rect() const;

public:
    explicit SplitContainer(const std::string& node_name = "Unnamed SplitContainer",
                            Vec2 node_position = {0, 0});

    void on_update_layout(const Rect& parent_rect) override;
    void on_draw(ConsoleRenderer& renderer, const Rect& clip) override;

    Vec2 get_minimum_size() const override;

    // --- Orientation --------------------------------------------------------

    void set_orientation(SplitOrientation new_orientation) { orientation = new_orientation; }
    SplitOrientation get_orientation() const { return orientation; }

    // --- Split --------------------------------------------------------------

    // Clamped to 0..1. Emits split_ratio_changed(float) only when the stored
    // ratio changes.
    void set_split_ratio(float new_ratio);
    float get_split_ratio() const { return split_ratio; }

    // The smallest extent each panel may take on the split axis.
    void set_min_first(int cells);
    int get_min_first() const { return min_first; }

    void set_min_second(int cells);
    int get_min_second() const { return min_second; }

    // --- Divider ------------------------------------------------------------

    // Zero or less collapses the divider; it is then not grabbable.
    void set_divider_width(int cells);
    int get_divider_width() const { return divider_width; }

    StyleBox& get_divider_stylebox() { return divider_stylebox; }
    const StyleBox& get_divider_stylebox() const { return divider_stylebox; }

protected:
    bool on_gui_input(const InputEvent& event) override;
};

} // namespace guayaba

#endif // GUAYABA_NODES_UI_SPLITCONTAINER_HPP
