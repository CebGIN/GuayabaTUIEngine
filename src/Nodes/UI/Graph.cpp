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
//
// The partial blocks are what buy sub-cell resolution: without them a one-row
// graph could only say "on" or "off".
const wchar_t* LADDER = L" \u2581\u2582\u2583\u2584\u2585\u2586\u2587\u2588";
constexpr int LEVELS_PER_ROW = 8;

// The glyph for one cell of a column that is `filled` eighths tall from the
// bottom, at `row` (0 is the top of the plot) out of `rows`.
wchar_t cell_glyph(int filled, int row, int rows, bool fill) {
    // Eighths accounted for by the rows below this one.
    const int from_below = (rows - 1 - row) * LEVELS_PER_ROW;
    const int remaining = filled - from_below;

    if (!fill) {
        // Sparkline: only the topmost non-empty cell of the column survives. The
        // row that cell lands in is derived from the fill so it does not matter
        // which rows are full underneath.
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
    // One row is the floor: a shorter graph could not show a level at all. The
    // width is the caller's floor unchanged, because the series scrolls its own
    // window rather than asking to be as wide as its history.
    const Vec2 own = Control::get_minimum_size();
    return {own.x, std::max(own.y, 1)};
}

// --- Data -------------------------------------------------------------------

void Graph::set_values(std::vector<double> new_values) {
    values = std::move(new_values);
}

void Graph::push_value(double value) {
    values.push_back(value);

    // A rolling history keeps the newest `capacity` samples and forgets the rest;
    // the oldest are the ones that fall off the left.
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

    // Newest at the right: the window keeps the last `columns` samples. When there
    // are fewer, begin stays at zero and the draw pushes the block right.
    const std::size_t window = static_cast<std::size_t>(columns);
    begin = values.size() > window ? values.size() - window : 0;
}

void Graph::effective_range(std::size_t begin, std::size_t end, double& lo,
                            double& hi) const {
    if (range_set) {
        lo = range_min;
        hi = range_max;
    } else if (begin >= end || values.empty()) {
        // No data to measure: a plain unit band, so filled_eighths has something
        // to divide by and nothing is drawn that depends on it anyway.
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

    // A caller can hand a fixed range the wrong way round; normalising here keeps
    // every draw path from having to care.
    if (hi < lo) std::swap(lo, hi);

    // A flat series (or a fixed range of zero width) has no span to normalise
    // against. Widening around the value puts the line through the middle of the
    // plot instead of collapsing it to the bottom or dividing by zero.
    if (hi - lo <= 0.0) {
        lo -= 0.5;
        hi += 0.5;
    }
}

int Graph::filled_eighths(double value, double lo, double hi, int rows) {
    if (rows <= 0) return 0;

    const double span = hi - lo;
    // effective_range guarantees span > 0, but a direct call may not: centring is
    // the same answer the widening band would give.
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

    // Right-aligned: the drawn block hugs the right edge, so the newest sample is
    // at the right border whatever the history length.
    const int count = static_cast<int>(end - begin);
    const int start_column = columns - count;

    // Column-major so the fill level is computed once per sample rather than once
    // per cell. Every cell is written, empty ones included, so the plot owns its
    // rect and no stale glyph survives from an earlier frame.
    for (int column = 0; column < columns; ++column) {
        int filled = 0;
        if (column >= start_column) {
            const std::size_t index =
                begin + static_cast<std::size_t>(column - start_column);
            filled = filled_eighths(values[index], lo, hi, rows);
        }

        for (int row = 0; row < rows; ++row) {
            const wchar_t glyph = cell_glyph(filled, row, rows, fill);
            // Per cell, because the glyph is decided by the sample's level and not
            // by a run of text - exactly the per-cell case put_char is for.
            renderer.put_char(Vec2{area.pos.x + column, area.pos.y + row}, glyph,
                              line_color, background_color, clip);
        }
    }

    Control::draw(renderer, clip);
}

} // namespace guayaba
