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
    A Control that plots one series of numbers as a bar chart.

    A terminal cell is half a character wide and a whole one tall, so vertical
    resolution comes from block glyphs: each row is split into eight sub-levels
    from a quarter-height bar up to a full block. H rows give H*8 levels; one row
    is the degenerate sparkline case.

        values = {2, 4, 4, 8, 3}          one row (sparkline):  ▂▄▄█▃

    values are oldest first. With more samples than columns the window keeps the
    newest; with fewer, the block is pushed to the right edge so the newest sample
    hugs the right border.

    No axes or border are drawn; the caller adds a frame (e.g. a PanelContainer)
    when one is wanted.

    By default the vertical span is the min and max of the samples on screen. A
    constant series would divide by a zero span, so a degenerate range is widened
    to a small band around the value. set_range() pins the span instead.
*/
class Graph : public Control {
public:
    explicit Graph(const std::string& node_name = "Unnamed Graph",
                   Vec2 node_position = {0, 0});

    void on_draw(ConsoleRenderer& renderer, const Rect& clip) override;

    // The caller's floor, at least one row tall. The width is not derived from the
    // data; the series scrolls its window.
    Vec2 get_minimum_size() const override;

    // --- Data ---------------------------------------------------------------

    // Oldest first, newest last. Drawn left to right, right-aligned.
    void set_values(std::vector<double> new_values);
    const std::vector<double>& get_values() const { return values; }

    // Appends one sample, dropping the oldest when a capacity is set.
    void push_value(double value);

    // Samples push_value keeps; 0 or less is unbounded. Enforced by push_value
    // only: set_values() stores what it is given.
    void set_capacity(int new_capacity) { capacity = new_capacity; }
    int get_capacity() const { return capacity; }

    // --- Range --------------------------------------------------------------

    // A fixed vertical span. Cleared means auto: the visible samples' own min/max.
    void set_range(double min, double max);
    void clear_range();

    bool has_range() const { return range_set; }

    // --- Appearance ---------------------------------------------------------

    // Fill (the default) draws solid bars up to the level; off draws only the top
    // cell of each column.
    void set_fill(bool enabled) { fill = enabled; }
    bool is_fill() const { return fill; }

    void set_line_color(uint16_t color) { line_color = color; }
    void set_line_color(const std::string& color_name);

    void set_background_color(uint16_t color) { background_color = color; }
    void set_background_color(const std::string& color_name);

protected:
    // The window a draw of `columns` cells shows: the newest columns samples, as
    // indices into values.
    void visible_range(int columns, std::size_t& begin, std::size_t& end) const;

    // The vertical span for slice [begin, end): the fixed range when set, otherwise
    // the slice's own min/max, widened when the two ends coincide.
    void effective_range(std::size_t begin, std::size_t end, double& lo, double& hi) const;

    // Eighths of a cell filled from the bottom for value within [lo, hi], over
    // `rows` rows.
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
