#ifndef GUAYABA_NODES_UI_PROGRESSBAR_HPP
#define GUAYABA_NODES_UI_PROGRESSBAR_HPP

#include <cstdint>
#include <string>
#include <utility>

#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/UI/Control.hpp>
#include <guayaba/Renderer/ConsoleRenderer.hpp>
#include <guayaba/Resources/Stylebox.hpp>

namespace guayaba {

/*
    A horizontal bar that fills in proportion to a value in [min, max].

    In indeterminate mode the value is ignored: a block a quarter of the bar wide
    slides from left to right on a loop, driven by the frame delta. Either way the
    text - a custom string, or the percentage when show_percentage is on - is
    centred over the bar.

    value_changed is emitted only when the effective (clamped) value changes, so a
    caller can connect it without filtering duplicates.
*/
class ProgressBar : public Control {
private:
    StyleBox stylebox;

    double min_value = 0.0;
    double max_value = 100.0;
    double value = 0.0;

    bool indeterminate = false;
    bool show_percentage = false;
    std::string text;

    uint16_t fill_color = 2;       // Green
    uint16_t background_color = 0; // Black
    uint16_t text_color = 15;      // Bright white

    // Seconds accumulated while indeterminate. The block's position is derived
    // from it rather than advanced per frame, so an irregular delta does not
    // accumulate drift.
    double phase = 0.0;

    // The rect inside the StyleBox frame, where the bar is painted.
    Rect content_rect() const;

    // Clamped to [min, max]; a degenerate range collapses to min.
    double clamp_value(double new_value) const;

    // 0..1 across the range, 0 when the range is degenerate.
    double ratio() const;

public:
    explicit ProgressBar(const std::string& node_name = "Unnamed ProgressBar",
                         Vec2 node_position = {0, 0});

    void on_update(double delta_time) override;

    void on_draw(ConsoleRenderer& renderer, const Rect& clip) override;

    Vec2 get_minimum_size() const override;

    // --- Value --------------------------------------------------------------

    // max <= min is a degenerate range: the bar draws empty and value collapses
    // to min. Re-clamps the current value and emits when it changes.
    void set_range(double min, double max);
    double get_min() const { return min_value; }
    double get_max() const { return max_value; }

    void set_value(double new_value);
    double get_value() const { return value; }

    // --- Mode ---------------------------------------------------------------

    void set_indeterminate(bool enabled) { indeterminate = enabled; }
    bool is_indeterminate() const { return indeterminate; }

    // --- Text ---------------------------------------------------------------

    // A custom label wins over the percentage.
    void set_text(std::string new_text) { text = std::move(new_text); }
    const std::string& get_text() const { return text; }

    void set_show_percentage(bool show) { show_percentage = show; }
    bool get_show_percentage() const { return show_percentage; }

    // --- Appearance ---------------------------------------------------------

    void set_fill_color(uint16_t color) { fill_color = color; }
    void set_background_color(uint16_t color) { background_color = color; }
    void set_text_color(uint16_t color) { text_color = color; }

    StyleBox& get_stylebox() { return stylebox; }
    const StyleBox& get_stylebox() const { return stylebox; }

    // Keys: "progress_bar", "progress_bar.fill", "progress_bar.background",
    // "progress_bar.text".
    void apply_theme(const Theme& theme) override;
};

} // namespace guayaba

#endif // GUAYABA_NODES_UI_PROGRESSBAR_HPP
