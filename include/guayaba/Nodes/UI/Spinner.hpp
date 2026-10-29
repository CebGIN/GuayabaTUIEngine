#ifndef GUAYABA_NODES_UI_SPINNER_HPP
#define GUAYABA_NODES_UI_SPINNER_HPP

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/UI/Control.hpp>
#include <guayaba/Renderer/ConsoleRenderer.hpp>
#include <guayaba/Resources/Stylebox.hpp>

namespace guayaba {

/*
    A one-cell animated glyph: the classic busy indicator.

    The frame advances when the accumulated delta reaches the interval, by as many
    frames as whole intervals have passed, so an irregular delta does not lose
    ticks. Stopped, it keeps showing the current frame; empty, it draws nothing.
*/
class Spinner : public Control {
private:
    StyleBox stylebox;

    std::vector<std::string> frames{"|", "/", "-", "\\"};
    size_t frame_index = 0;

    double interval = 0.1;
    double accumulator = 0.0;
    bool running = true;

    uint16_t text_color = 7;       // White, the TermChar default
    uint16_t background_color = 0; // Black, the TermChar default

    // The rect inside the StyleBox frame, where the glyph is drawn.
    Rect content_rect() const;

public:
    explicit Spinner(const std::string& node_name = "Unnamed Spinner",
                     Vec2 node_position = {0, 0});

    void on_update(double delta_time) override;

    void on_draw(ConsoleRenderer& renderer, const Rect& clip) override;

    Vec2 get_minimum_size() const override;

    // --- Frames -------------------------------------------------------------

    // An empty list draws nothing. The current frame is kept modulo the new size.
    void set_frames(std::vector<std::string> new_frames);
    const std::vector<std::string>& get_frames() const { return frames; }

    // --- Timing -------------------------------------------------------------

    // Seconds between frames. Zero or less restores the 0.1 s default.
    void set_interval(double seconds);
    double get_interval() const { return interval; }

    void start() { running = true; }
    void stop() { running = false; }
    void set_running(bool value) { running = value; }
    bool is_running() const { return running; }

    // --- Appearance ---------------------------------------------------------

    void set_text_color(uint16_t color) { text_color = color; }
    void set_background_color(uint16_t color) { background_color = color; }

    StyleBox& get_stylebox() { return stylebox; }
    const StyleBox& get_stylebox() const { return stylebox; }

    // Keys: "spinner", "text.primary".
    void apply_theme(const Theme& theme) override;
};

} // namespace guayaba

#endif // GUAYABA_NODES_UI_SPINNER_HPP
