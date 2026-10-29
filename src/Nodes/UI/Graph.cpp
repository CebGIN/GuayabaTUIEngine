#include <algorithm>
#include <cmath>
#include <utility>

#include <guayaba/Core/Color.hpp>
#include <guayaba/Nodes/UI/Graph.hpp>

namespace guayaba {

namespace {

// The vertical ladder, one glyph per eighth of a cell, index 0 (empty) through 8
// (full block):
//
//     0 ' '  1 ▁  2 ▂  3 ▃  4 ▄  5 ▅  6 ▆  7 ▇  8 █
const wchar_t* LADDER = L" \u2581\u2582\u2583\u2584\u2585\u2586\u2587\u2588";
constexpr int LEVELS_PER_ROW = 8;

// The glyph for one cell of a column that is `filled` eighths tall from the
// bottom, at `row` (0 is the top of the plot) out of `rows`.
wchar_t cell_glyph(int filled, int row, int rows, bool fill) {
    // Eighths accounted for by the rows below this one.
    const int from_below = (rows - 1 - row) * LEVELS_PER_ROW;
    const int remaining = filled - from_below;

    if (!fill) {
        // Sparkline: only the topmost non-empty cell survives, derived from the
        // fill.
        if (filled <= 0) return L' ';
        const int top_row = rows - 1 - (filled - 1) / LEVELS_PER_ROW;
        if (row != top_row) return L' ';
    }

    if (remaining <= 0) return L' ';
    if (remaining >= LEVELS_PER_ROW) return LADDER[LEVELS_PER_ROW];
    return LADDER[remaining];
}

} // namespace

Graph::Graph(const std::string& node_name, Vec2 node_position)
    : Control(node_name, node_position) {}

// --- Size -------------------------------------------------------------------

Vec2 Graph::get_minimum_size() const {
    // One row is the floor; the width is the caller's floor unchanged, since the
    // series scrolls its own window.
    const Vec2 own = Control::get_minimum_size();
    return {own.x, std::max(own.y, 1)};
}

// --- Data -------------------------------------------------------------------

void Graph::set_values(std::vector<double> new_values) {
    values = std::move(new_values);
}

void Graph::push_value(double value) {
    values.push_back(value);

    // Keeps the newest `capacity` samples; the oldest fall off the left.
    if (capacity > 0 && static_cast<int>(values.size()) > capacity) {
        const std::size_t excess = values.size() - static_cast<std::size_t>(capacity);
        values.erase(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(excess));
    }
}

// --- Range ------------------------------------------------------------------

void Graph::set_range(double min, double max) {
    range_set = true;
    range_min = min;
    range_max = max;
}

void Graph::clear_range() { range_set = false; }

void Graph::set_line_color(const std::string& color_name) {
    line_color = Color::from_name(color_name);
}

void Graph::set_background_color(const std::string& color_name) {
    background_color = Color::from_name(color_name);
}

// --- Mapping ----------------------------------------------------------------

void Graph::visible_range(int columns, std::size_t& begin, std::size_t& end) const {
    end = values.size();
    if (columns <= 0) {
        begin = end;
        return;
    }

    // The window keeps the last `columns` samples; with fewer, begin stays at zero
    // and the draw pushes the block right.
    const std::size_t window = static_cast<std::size_t>(columns);
    begin = values.size() > window ? values.size() - window : 0;
}

void Graph::effective_range(std::size_t begin, std::size_t end, double& lo,
                            double& hi) const {
    if (range_set) {
        lo = range_min;
        hi = range_max;
    } else if (begin >= end || values.empty()) {
        // No data: a plain unit band so filled_eighths has a divisor.
        lo = 0.0;
        hi = 1.0;
    } else {
        lo = values[begin];
        hi = values[begin];
        for (std::size_t i = begin; i < end; ++i) {
            lo = std::min(lo, values[i]);
            hi = std::max(hi, values[i]);
        }
    }

    // Normalises a fixed range handed the wrong way round.
    if (hi < lo) std::swap(lo, hi);

    // A flat series has no span to normalise against, so widen around the value
    // and put the line through the middle.
    if (hi - lo <= 0.0) {
        lo -= 0.5;
        hi += 0.5;
    }
}

int Graph::filled_eighths(double value, double lo, double hi, int rows) {
    if (rows <= 0) return 0;

    const double span = hi - lo;
    // effective_range guarantees span > 0, but a direct call need not: centring
    // matches the widened band.
    double normalized = span > 0.0 ? (value - lo) / span : 0.5;
    normalized = std::clamp(normalized, 0.0, 1.0);

    return static_cast<int>(std::lround(normalized * rows * LEVELS_PER_ROW));
}

// --- Drawing ----------------------------------------------------------------

void Graph::draw(ConsoleRenderer& renderer, const Rect& clip) {
    const Rect area = get_rect();
    if (area.is_empty()) {
        Control::draw(renderer, clip);
        return;
    }

    const int columns = area.size.x;
    const int rows = area.size.y;

    std::size_t begin = 0;
    std::size_t end = 0;
    visible_range(columns, begin, end);

    double lo = 0.0;
    double hi = 1.0;
    effective_range(begin, end, lo, hi);

    // Right-aligned: the newest sample hugs the right border whatever the history
    // length.
    const int count = static_cast<int>(end - begin);
    const int start_column = columns - count;

    // Column-major so the fill is computed once per sample. Every cell is written,
    // empty ones included, so no stale glyph survives from an earlier frame.
    for (int column = 0; column < columns; ++column) {
        int filled = 0;
        if (column >= start_column) {
            const std::size_t index =
                begin + static_cast<std::size_t>(column - start_column);
            filled = filled_eighths(values[index], lo, hi, rows);
        }

        for (int row = 0; row < rows; ++row) {
            const wchar_t glyph = cell_glyph(filled, row, rows, fill);
            // The glyph depends on the sample's level, not on a run of text.
            renderer.put_char(Vec2{area.pos.x + column, area.pos.y + row}, glyph,
                              line_color, background_color, clip);
        }
    }

    Control::draw(renderer, clip);
}

} // namespace guayaba
