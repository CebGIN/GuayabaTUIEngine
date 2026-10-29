#include <algorithm>
#include <cmath>
#include <string>

#include <guayaba/Core/Utf8.hpp>
#include <guayaba/Nodes/UI/ProgressBar.hpp>
#include <guayaba/Resources/Theme.hpp>

namespace guayaba {

namespace {

// Cells per second for the indeterminate block. Fixed rather than exposed: the
// mode is decorative, and a caller that needs a different speed can animate the
// value itself.
constexpr double INDETERMINATE_CELLS_PER_SECOND = 12.0;

} // namespace

ProgressBar::ProgressBar(const std::string& node_name, Vec2 node_position)
    : Control(node_name, node_position) {
    declare_signal<double>("value_changed");
}

double ProgressBar::clamp_value(double new_value) const {
    if (max_value <= min_value) return min_value;
    return std::clamp(new_value, min_value, max_value);
}

double ProgressBar::ratio() const {
    if (max_value <= min_value) return 0.0;
    return std::clamp((value - min_value) / (max_value - min_value), 0.0, 1.0);
}

void ProgressBar::set_range(double min, double max) {
    min_value = min;
    max_value = max;

    const double effective = clamp_value(value);
    if (effective == value) return;

    value = effective;
    emit_signal("value_changed", value);
}

void ProgressBar::set_value(double new_value) {
    const double effective = clamp_value(new_value);
    if (effective == value) return;

    value = effective;
    emit_signal("value_changed", value);
}

Rect ProgressBar::content_rect() const {
    return stylebox.get_content_rect(get_rect());
}

Vec2 ProgressBar::get_minimum_size() const {
    const Vec2 frame = stylebox.get_frame_size();
    const Vec2 own = Control::get_minimum_size();
    return {
        std::max(own.x, frame.x + 10),
        std::max(own.y, frame.y + 1),
    };
}

void ProgressBar::on_update(double delta_time) {
    // The wrapper already stopped a hidden bar; only the animation clock is ours.
    if (indeterminate && delta_time > 0.0) phase += delta_time;

    Node::on_update(delta_time);
}

void ProgressBar::on_draw(ConsoleRenderer& renderer, const Rect& clip) {
    stylebox.apply(renderer, get_rect(), clip);

    const Rect content = content_rect();
    if (!content.is_empty()) {
        // The unfilled track first, then the filled part over it.
        renderer.fill_rect(content, L' ', text_color, background_color, clip);

        if (indeterminate) {
            const int block = std::max(1, content.size.x / 4);
            const int span = content.size.x + block;
            const int offset = static_cast<int>(
                std::fmod(phase * INDETERMINATE_CELLS_PER_SECOND, static_cast<double>(span)));

            // The block starts off the left edge and leaves off the right one, so
            // it enters and exits instead of jumping.
            const int start = offset - block;
            const int from = std::max(0, start);
            const int to = std::min(content.size.x, start + block);
            if (from < to) {
                renderer.fill_rect(
                    Rect{Vec2{content.pos.x + from, content.pos.y},
                         Vec2{to - from, content.size.y}},
                    L'█', fill_color, background_color, clip);
            }
        } else {
            const int filled = std::clamp(
                static_cast<int>(std::floor(content.size.x * ratio())), 0, content.size.x);
            if (filled > 0) {
                renderer.fill_rect(
                    Rect{content.pos, Vec2{filled, content.size.y}},
                    L'█', fill_color, background_color, clip);
            }
        }

        std::string label = text;
        if (label.empty() && show_percentage) {
            label = std::to_string(static_cast<int>(std::lround(ratio() * 100.0))) + "%";
        }
        if (!label.empty()) {
            const int label_width = text_width(label);
            const int x = content.pos.x + std::max(0, (content.size.x - label_width) / 2);
            renderer.put_text(Vec2{x, content.pos.y}, {label}, text_color,
                              background_color, content.intersection(clip));
        }
    }

    Control::on_draw(renderer, clip);
}

void ProgressBar::apply_theme(const Theme& theme) {
    stylebox = theme.get_style("progress_bar", stylebox);
    fill_color = theme.get_color("progress_bar.fill", fill_color);
    background_color = theme.get_color("progress_bar.background", background_color);
    text_color = theme.get_color("progress_bar.text", text_color);
}

} // namespace guayaba
