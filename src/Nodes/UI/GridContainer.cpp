#include <algorithm>
#include <memory>
#include <vector>

#include <guayaba/Nodes/UI/GridContainer.hpp>

namespace guayaba {

namespace {

// The grid as measured from the children's minimums, before any slack is
// distributed. column_widths always has `columns` entries; row_heights has one
// per used row. `cells` is how many Control children take part.
struct GridMetrics {
    std::vector<int> column_widths;
    std::vector<int> row_heights;
    std::vector<bool> column_grows;
    std::vector<bool> row_grows;
    size_t cells = 0;
};

GridMetrics measure(const std::vector<std::unique_ptr<Node>>& children, int columns) {
    GridMetrics metrics;
    metrics.column_widths.assign(static_cast<size_t>(columns), 0);
    metrics.column_grows.assign(static_cast<size_t>(columns), false);

    size_t cell = 0;
    for (const std::unique_ptr<Node>& child : children) {
        if (!child->is_visible()) continue;

        const Control* control = dynamic_cast<const Control*>(child.get());
        if (control == nullptr) continue;  // not part of the grid

        const size_t column = cell % static_cast<size_t>(columns);
        const size_t row = cell / static_cast<size_t>(columns);
        if (row >= metrics.row_heights.size()) {
            metrics.row_heights.resize(row + 1, 0);
            metrics.row_grows.resize(row + 1, false);
        }

        const Vec2 minimum = control->get_minimum_size();
        metrics.column_widths[column] = std::max(metrics.column_widths[column], minimum.x);
        metrics.row_heights[row] = std::max(metrics.row_heights[row], minimum.y);

        if (control->get_grow_x() == Grow::Both) metrics.column_grows[column] = true;
        if (control->get_grow_y() == Grow::Both) metrics.row_grows[row] = true;

        ++cell;
    }

    metrics.cells = cell;
    return metrics;
}

// How many columns actually hold a child. The rest are empty and must not add a
// separation to the minimum.
size_t used_columns(const GridMetrics& metrics, int columns) {
    return std::min<size_t>(static_cast<size_t>(columns), metrics.cells);
}

int extent_of(const std::vector<int>& sizes, size_t used, int separation) {
    int total = 0;
    for (size_t i = 0; i < used; ++i) total += sizes[i];
    if (used > 1) total += separation * static_cast<int>(used - 1);
    return total;
}

// Splits `slack` between the growers, remainder to the first. Adds the share to
// each growing size in place.
int distribute(int slack, const std::vector<bool>& grows, size_t used,
               std::vector<int>& sizes) {
    int growers = 0;
    for (size_t i = 0; i < used; ++i) {
        if (grows[i]) ++growers;
    }
    if (growers == 0) return 0;

    const int share = slack / growers;
    const int remainder = slack % growers;

    int seen = 0;
    for (size_t i = 0; i < used; ++i) {
        if (!grows[i]) continue;
        sizes[i] += share;
        if (seen < remainder) ++sizes[i];
        ++seen;
    }
    return growers;
}

} // namespace

GridContainer::GridContainer(const std::string& node_name, Vec2 node_position)
    : Container(node_name, node_position) {}

void GridContainer::set_columns(int new_columns) {
    columns = new_columns <= 0 ? 1 : new_columns;
}

void GridContainer::set_h_separation(int cells) {
    h_separation = cells < 0 ? 0 : cells;
}

void GridContainer::set_v_separation(int cells) {
    v_separation = cells < 0 ? 0 : cells;
}

Vec2 GridContainer::get_minimum_size() const {
    const GridMetrics metrics = measure(children, columns);
    const size_t used = used_columns(metrics, columns);

    const Vec2 own = Control::get_minimum_size();
    return {
        std::max(own.x, extent_of(metrics.column_widths, used, h_separation)),
        std::max(own.y, extent_of(metrics.row_heights, metrics.row_heights.size(), v_separation)),
    };
}

void GridContainer::on_update_layout(const Rect& parent_rect) {
    resolve_rect(parent_rect);

    const Rect box = get_rect();

    // The block is the minimum-sized grid placed by content_alignment; the
    // minimum is recomputed from the children's own state, so this does not
    // depend on the rects the previous pass left behind.
    const Rect block = content_rect();

    const GridMetrics metrics = measure(children, columns);
    const size_t used = used_columns(metrics, columns);

    std::vector<int> widths = metrics.column_widths;
    std::vector<int> heights = metrics.row_heights;

    const int slack_x = std::max(0, box.size.x - block.size.x);
    const int slack_y = std::max(0, box.size.y - block.size.y);

    const int column_growers =
        distribute(slack_x, metrics.column_grows, used, widths);
    const int row_growers =
        distribute(slack_y, metrics.row_grows, heights.size(), heights);

    // With a grower on an axis the grid spans the box from its origin; without
    // one it stays where content_rect() aligned it.
    const int start_x = column_growers > 0 ? box.pos.x : block.pos.x;
    const int start_y = row_growers > 0 ? box.pos.y : block.pos.y;

    std::vector<int> column_x(used, 0);
    int x = start_x;
    for (size_t c = 0; c < used; ++c) {
        column_x[c] = x;
        x += widths[c] + h_separation;
    }

    std::vector<int> row_y(heights.size(), 0);
    int y = start_y;
    for (size_t r = 0; r < heights.size(); ++r) {
        row_y[r] = y;
        y += heights[r] + v_separation;
    }

    // The rect a non-Control child gets: the whole grid, slack included.
    const Rect grid_rect{
        Vec2{start_x, start_y},
        Vec2{extent_of(widths, used, h_separation),
             extent_of(heights, heights.size(), v_separation)},
    };

    size_t cell = 0;
    for (size_t i = 0; i < children.size(); ++i) {
        Node* child = children[i].get();
        if (!child->is_visible()) continue;

        if (dynamic_cast<const Control*>(child) == nullptr) {
            place_child(i, grid_rect);
            continue;
        }

        const size_t column = cell % static_cast<size_t>(columns);
        const size_t row = cell / static_cast<size_t>(columns);
        ++cell;

        place_child(i, Rect{
            Vec2{column_x[column], row_y[row]},
            Vec2{widths[column], heights[row]},
        });
    }
}

} // namespace guayaba
