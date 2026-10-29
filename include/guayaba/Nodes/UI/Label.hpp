#ifndef GUAYABA_NODES_UI_LABEL_HPP
#define GUAYABA_NODES_UI_LABEL_HPP

#include <cstdint>
#include <string>
#include <vector>

#include <guayaba/Core/Rect.hpp>
#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/UI/Control.hpp>
#include <guayaba/Renderer/ConsoleRenderer.hpp>
#include <guayaba/Resources/Stylebox.hpp>

namespace guayaba {

/*
    A Control that shows word-wrapped text inside a StyleBox. Unlike
    NodeTextMatrix it is laid out, so it takes part in anchors and clipping.

    The text is held twice: raw_text is what the caller set, wrapped_lines is
    wrap() of raw_text at the resolved content width and what is drawn.

    Height comes from the anchors, not the wrap, so excess lines are clipped.

    fit_content is the exception: when on, the Label's minimum is the text's own
    extent (measured from raw_text, independent of the size handed out) so a
    container can make room for it. Either way the StyleBox frame is added on top.
*/
class Label : public Control {
protected:
    StyleBox stylebox;

    std::string raw_text;
    std::vector<std::string> wrapped_lines;

    uint16_t text_color = 7;      // White, the TermChar default
    uint16_t background_color = 0; // Black, the TermChar default

    // Whether this Label reports the size its text wants.
    bool fit_content = false;

    // Cached text extent; get_minimum_size() runs every layout pass.
    mutable Vec2 measured_size{0, 0};
    mutable bool measured_size_valid = false;

    // Extent of raw_text, cached; delegates to TextWrap::measure_text.
    Vec2 measure_text() const;

    // The box's rect minus its border and margins; empty when the box is too small.
    Rect content_rect() const;

    // The region the text is drawn into: the incoming clip's x-range and the
    // label's own y-range, so horizontal overflow is left to an ancestor and
    // vertical overflow is cut at the label's bottom.
    Rect text_clip(const Rect& incoming) const;

public:
    explicit Label(const std::string& node_name = "Unnamed Label", Vec2 node_position = {0, 0});

    void update_layout(const Rect& parent_rect) override;

    void draw(ConsoleRenderer& renderer, const Rect& clip) override;

    // --- Size ----------------------------------------------------------------

    // The text extent plus StyleBox frame when fit_content is on; otherwise the imposed size.
    Vec2 get_minimum_size() const override;

    // --- Fit content ---------------------------------------------------------

    bool get_fit_content() const { return fit_content; }

    // On: the Label reports the text's extent as its minimum. Off: a zero minimum.
    void set_fit_content(bool enabled) {
        if (fit_content == enabled) return;
        fit_content = enabled;
        measured_size_valid = false;
        invalidate_cache_recursively();
    }

    // --- Text ---------------------------------------------------------------

    // Re-wraps against the current content width immediately.
    void set_text(const std::string& text);

    const std::string& get_text() const { return raw_text; }

    // The lines as last wrapped. Derived state; set text through set_text().
    const std::vector<std::string>& get_wrapped_lines() const { return wrapped_lines; }

    void set_text_color(uint16_t color) { text_color = color; }
    void set_text_color(const std::string& color_name) { text_color = Color::from_name(color_name); }

    void set_background_color(uint16_t color) { background_color = color; }
    void set_background_color(const std::string& color_name) {
        background_color = Color::from_name(color_name);
    }

    // --- Appearance ---------------------------------------------------------

    StyleBox& get_stylebox() { return stylebox; }
    const StyleBox& get_stylebox() const { return stylebox; }

    // A StyleBox change moves the content rect, so the wrap is recomputed.
    void set_stylebox(StyleBox new_stylebox);
};

} // namespace guayaba

#endif // GUAYABA_NODES_UI_LABEL_HPP