#ifndef GUAYABA_NODES_UI_SPINBOX_HPP
#define GUAYABA_NODES_UI_SPINBOX_HPP

#include <string>

#include <guayaba/Core/InputEvent.hpp>
#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/UI/LineEdit.hpp>

namespace guayaba {

/*
    A LineEdit that edits a number.

    The two rightmost columns are reserved for the up/down arrows: the text area
    shrinks by that much through the StyleBox's right content margin, so the
    inherited minimum already counts them.

    While typing, a complete number updates the value live (without reformatting,
    so the caret does not jump). Enter and losing the focus clamp and rewrite the
    text with the configured decimals. Up/Down step by `step`, PageUp/PageDown by
    ten steps, and a click on the arrow strip does the same. A degenerate range
    (`max <= min`) leaves the control read-only at `min`.
*/
class SpinBox : public LineEdit {
private:
    double min_value = 0.0;
    double max_value = 100.0;
    double value = 0.0;
    double step = 1.0;

    // Zero or less normalizes to 0; capped at 15, the useful precision of a double.
    int decimals = 0;

    bool wrap = false;

    // True while reformat() writes the text, so the text_changed reactor does not
    // parse the control's own output.
    bool formatting = false;

    double clamp_value(double new_value) const;

    // Rounds to `decimals` places.
    double snap(double new_value) const;

    std::string format_value(double new_value) const;

    // Parses a complete number, with the classic locale, and rejects leftovers.
    static bool parse_number(const std::string& input, double& out);

    // Live path: clamps and emits, but leaves the text as typed.
    void set_value_live(double new_value);

    // Writes the current value as text and puts the caret at the end.
    void reformat();

    bool step_by(double delta);

    bool is_read_only() const { return max_value <= min_value; }

    // The two reserved columns, just right of the text area.
    Rect arrow_zone() const;

public:
    explicit SpinBox(const std::string& node_name = "Unnamed SpinBox",
                     Vec2 node_position = {0, 0});

    void on_draw(ConsoleRenderer& renderer, const Rect& clip) override;

    // --- Range and step -----------------------------------------------------

    // max <= min leaves the control read-only at min.
    void set_range(double min, double max);
    double get_min() const { return min_value; }
    double get_max() const { return max_value; }

    // Zero or less normalizes to 1e-9, a positive step small enough to be a
    // no-op in practice but still valid.
    void set_step(double new_step);
    double get_step() const { return step; }

    // --- Format -------------------------------------------------------------

    // Negative values normalize to 0; values above 15 are capped.
    void set_decimals(int new_decimals);
    int get_decimals() const { return decimals; }

    // --- Value --------------------------------------------------------------

    // Clamps and rounds, rewrites the text, and emits value_changed only when the
    // effective value changes.
    void set_value(double new_value);
    double get_value() const { return value; }

    // Passing the maximum steps to the minimum and vice versa.
    void set_wrap(bool enabled) { wrap = enabled; }
    bool get_wrap() const { return wrap; }

protected:
    bool on_gui_input(const InputEvent& event) override;
};

} // namespace guayaba

#endif // GUAYABA_NODES_UI_SPINBOX_HPP
