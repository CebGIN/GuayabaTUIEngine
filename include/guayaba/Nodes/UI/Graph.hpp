#ifndef GUAYABA_NODES_UI_GRAPH_HPP
#define GUAYABA_NODES_UI_GRAPH_HPP

#include <cstdint>
#include <string>
#include <vector>

#include <guayaba/Core/Rect.hpp>
#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/UI/Control.hpp>
#include <guayaba/Renderer/ConsoleRenderer.hpp>

namespace guayaba {

/*
    A Control that plots one series of numbers as a bar chart or a sparkline.

    A terminal cell is half a character wide and a whole one tall, so vertical
    resolution has to come from the block glyphs: each row is split into eight
    sub-levels by the ladder from a quarter-height bar up to a full block.

        values = {2, 4, 4, 8, 3}          one row (sparkline):  ▂▄▄█▃
        values = {2, 4, 4, 8, 3}          four rows:           ▁
                                                               ▆
                                                               █
                                                               █▇█

    The height of the widget decides how much resolution a level buys: H rows is
    H*8 levels, and one row is the degenerate sparkline case where every sample
    is a single partial block.

    RIGHT-ALIGNED, NEWEST LAST
    values are oldest first. When there are more samples than columns the window
    keeps the newest ones, and when there are fewer the block is pushed to the
    right edge so the newest sample always hugs the right border. That is what a
    rolling history wants: the clock hand is fixed at the right and the past
    slides off the left, rather than every sample shifting sideways as the series
    grows.

    NO AXES, NO LABELS
    This is a plot, not a chart. The caller knows what the numbers mean and where
    the caption goes, so the widget spends every cell on the data and draws no
    border of its own. Put it in a PanelContainer when the frame matters.

    THE RANGE
    By default the vertical span is the min and max of the samples actually on
    screen, so the plot uses its full height whatever the units are. A constant
    series would divide by a zero span, so a degenerate range is widened to a
    small band around the value: a flat line sits through the middle instead of
    vanishing. set_range() pins the span instead, which is what a CPU percentage
    wants - 0..100 so a quiet machine reads as a low line rather than a full
    range of noise.

    There is no StyleBox: the widget draws into its own rect with the line and
    background colours, and a frame is something the caller adds around it.
*/
class Graph : public Control {
public:
    explicit Graph(const std::string& node_name = "Unnamed Graph",
                   Vec2 node_position = {0, 0});

    void draw(ConsoleRenderer& renderer, const Rect& clip) override;

    // The caller's floor, at least one row tall. There is no frame to add - the
    // widget draws edge to edge - and, as with every Control, the width is not
    // derived from the data: a graph is as wide as it is told and the series just
    // scrolls its window.
    Vec2 get_minimum_size() const override;

    // --- Data ---------------------------------------------------------------

    // Oldest first, newest last. Drawn left to right, right-aligned.
    void set_values(std::vector<double> new_values);
    const std::vector<double>& get_values() const { return values; }

    // Appends one sample, dropping the oldest when a capacity is set.
    void push_value(double value);

    // How many samples push_value keeps. 0 or less is unbounded. The limit is
    // enforced by push_value only; set_values() is an explicit override and stores
    // what it is given whatever the capacity says.
    void set_capacity(int new_capacity) { capacity = new_capacity; }
    int get_capacity() const { return capacity; }

    // --- Range --------------------------------------------------------------

    // A fixed vertical span. Cleared means auto: the visible samples' own min/max.
    void set_range(double min, double max);
    void clear_range();

    bool has_range() const { return range_set; }

    // --- Appearance ---------------------------------------------------------

    // Fill (the default) draws solid bars up to the level; off draws only the top
    // cell of each column, which is the sparkline look at any height.
    void set_fill(bool enabled) { fill = enabled; }
    bool is_fill() const { return fill; }

    void set_line_color(uint16_t color) { line_color = color; }
    void set_line_color(const std::string& color_name);

    void set_background_color(uint16_t color) { background_color = color; }
    void set_background_color(const std::string& color_name);

protected:
    // The window of samples a draw of `columns` cells would show: the newest
    // columns samples. begin/end are indices into values and satisfy
    // end - begin <= columns.
    void visible_range(int columns, std::size_t& begin, std::size_t& end) const;

    // The vertical span the slice [begin, end) is drawn against: the fixed range
    // when one is set, otherwise the slice's own min/max, widened to a small band
    // when the two ends coincide so a constant series does not divide by zero.
    void effective_range(std::size_t begin, std::size_t end, double& lo, double& hi) const;

    // Eighths of a cell filled from the bottom for value within [lo, hi], over
    // `rows` rows. The mapping is the whole widget and cannot be read back off the
    // renderer, so it is a named piece a subclass - or the headless test - can
    // reach.
    static int filled_eighths(double value, double lo, double hi, int rows);

private:
    std::vector<double> values;

    int capacity = 0;

    bool range_set = false;
    double range_min = 0.0;
    double range_max = 0.0;

    bool fill = true;
    uint16_t line_color = 7;        // White, the TermChar default
    uint16_t background_color = 0;  // Black, the TermChar default
};

} // namespace guayaba

#endif // GUAYABA_NODES_UI_GRAPH_HPP
