#ifndef GUAYABA_NODES_UI_GRIDCONTAINER_HPP
#define GUAYABA_NODES_UI_GRIDCONTAINER_HPP

#include <string>

#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/UI/Container.hpp>

namespace guayaba {

/*
    A Container that lays its Control children out row by row in a fixed number of
    columns. A column is as wide as the widest minimum in it and a row as tall as
    the tallest, so the cells line up without the children having to agree.

    Leftover space goes only to columns or rows that contain a child with
    Grow::Both on that axis, split equally and with the remainder to the first
    grower - the same rule HBox and VBox use.

    Non-Control children are not part of the grid: they get the whole grid rect,
    as in a plain Container, and do not contribute to the minimum. Hidden children
    are skipped entirely.
*/
class GridContainer : public Container {
private:
    int columns = 1;
    int h_separation = 0;
    int v_separation = 0;

public:
    explicit GridContainer(const std::string& node_name = "Unnamed GridContainer",
                           Vec2 node_position = {0, 0});

    void on_update_layout(const Rect& parent_rect) override;

    Vec2 get_minimum_size() const override;

    // --- Columns ------------------------------------------------------------

    // Zero or less normalizes to 1.
    void set_columns(int new_columns);
    int get_columns() const { return columns; }

    // --- Separations --------------------------------------------------------

    // Gaps between columns/rows, floored at zero.
    void set_h_separation(int cells);
    int get_h_separation() const { return h_separation; }

    void set_v_separation(int cells);
    int get_v_separation() const { return v_separation; }
};

} // namespace guayaba

#endif // GUAYABA_NODES_UI_GRIDCONTAINER_HPP
