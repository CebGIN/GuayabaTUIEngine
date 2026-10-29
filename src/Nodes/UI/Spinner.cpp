#include <algorithm>
#include <cmath>

#include <guayaba/Nodes/UI/Spinner.hpp>
#include <guayaba/Resources/Theme.hpp>

namespace guayaba {

namespace {

constexpr double DEFAULT_INTERVAL = 0.1;

} // namespace

Spinner::Spinner(const std::string& node_name, Vec2 node_position)
    : Control(node_name, node_position) {}

void Spinner::set_frames(std::vector<std::string> new_frames) {
    frames = std::move(new_frames);

    if (frames.empty()) {
        frame_index = 0;
    } else {
        frame_index %= frames.size();
    }
}

void Spinner::set_interval(double seconds) {
    interval = seconds > 0.0 ? seconds : DEFAULT_INTERVAL;
}

Rect Spinner::content_rect() const {
    return stylebox.get_content_rect(get_rect());
}

Vec2 Spinner::get_minimum_size() const {
    const Vec2 frame = stylebox.get_frame_size();
    const Vec2 own = Control::get_minimum_size();
    return {
        std::max(own.x, frame.x + 1),
        std::max(own.y, frame.y + 1),
    };
}

void Spinner::on_update(double delta_time) {
    // The wrapper already stopped a hidden spinner; only the clock is ours.
    if (running && !frames.empty() && delta_time > 0.0) {
        accumulator += delta_time;

        if (accumulator >= interval) {
            const double steps = std::floor(accumulator / interval);

            // Reduce before casting: a huge delta would otherwise overflow the
            // integer conversion.
            const size_t advance = static_cast<size_t>(
                std::fmod(steps, static_cast<double>(frames.size())));
            frame_index = (frame_index + advance) % frames.size();
            accumulator = std::fmod(accumulator, interval);
        }
    }

    Node::on_update(delta_time);
}

void Spinner::on_draw(ConsoleRenderer& renderer, const Rect& clip) {
    stylebox.apply(renderer, get_rect(), clip);

    const Rect content = content_rect();
    if (!frames.empty() && !content.is_empty()) {
        renderer.put_text(content.pos, {frames[frame_index]}, text_color, background_color,
                          content.intersection(clip));
    }

    Control::on_draw(renderer, clip);
}

void Spinner::apply_theme(const Theme& theme) {
    stylebox = theme.get_style("spinner", stylebox);
    text_color = theme.get_color("text.primary", text_color);
}

} // namespace guayaba
