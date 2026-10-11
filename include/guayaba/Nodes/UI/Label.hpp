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
    A Control that shows text inside a StyleBox.

    Differs from NodeTextMatrix in the two things a text node usually wants:
    it is laid out (so it participates in anchors and clipping rather than
    living at a raw global position), and its text is word-wrapped to whatever
    width the anchors give it.

    The text is held twice, and the distinction matters:

        raw_text       what the caller set - the source of truth
        wrapped_lines  derived by wrap() from raw_text at the resolved content
                      width - what actually gets drawn

    Height comes from the anchors, not from the wrap, so a Label can be made
    shorter than its text needs and the excess lines are clipped. That also
    means the wrap never feeds back into size, so there is no circularity
    between wrapping and measuring.

    fit_content is the exception, and it is measured rather than wrapped. A
    fitting Label reports the size its text wants instead of a minimum of zero,
    so a container can make room for it:

        fit_content = false   no minimum of its own; wraps into whatever the
                              layout gives it, and clips the overflow. The
                              default, and what a Label in a fixed frame wants.

        fit_content = true    the minimum is the text's own extent: as wide as
                              its longest line, and as tall as the lines it
                              takes. Measured from raw_text alone, so the answer
                              is the same whatever size the layout hands out -
                              a Label squeezed narrow reports what the text
                              needs, not what it was squeezed into.

    The minimum is a floor, not a target: set_size() and the anchors still say
    how big the Label actually gets, and this only stops it getting smaller than
    the text.

    Either way the sizebox frame is added on top, so the text fits inside the
    border and margins rather than under them.
*/
class Label : public Control {
protected:
    StyleBox stylebox;

    std::string raw_text;
    std::vector<std::string> wrapped_lines;

    uint16_t text_color = 7;      // White, the TermChar default
    uint16_t background_color = 0; // Black, the TermChar default

    // Whether this Label reports the size its text wants. See the class comment.
    bool fit_content = false;

    // Measured when fit_content is on, cached because get_minimum_size() runs on
    // every layout pass and the text does not change between passes.
    mutable Vec2 measured_size{0, 0};
    mutable bool measured_size_valid = false;

    // The extent of raw_text, cached. Delegates to TextWrap::measure_text; the
    // wrapper exists so the cached value lives next to the text it came from.
    Vec2 measure_text() const;

    // Where the text goes: the box's rect minus its border and margins. Empty
    // when the box is too small to hold anything, which the draw treats as
    // "draw no text" rather than as something to compensate for.
    Rect content_rect() const;

    // The region the text is drawn into. Horizontal overflow is left to an
    // ancestor to clip - a word too wide for the box spills and the nearest
    // clipping ancestor cuts it - while vertical overflow is cut here, at the
    // label's own bottom, once clip_children says the label is a boundary.
    //
    // Keeping the incoming clip's x-range but taking the label's y-range is what
    // makes those two axes behave differently on purpose.
    Rect text_clip(const Rect& incoming) const;

public:
    explicit Label(const std::string& node_name = "Unnamed Label", Vec2 node_position = {0, 0});

    void update_layout(const Rect& parent_rect) override;

    void draw(ConsoleRenderer& renderer, const Rect& clip) override;

    // --- Size ----------------------------------------------------------------

    // With fit_content on, this is the text's own extent plus the stylebox
    // frame; with it off, it is whatever the caller imposed.
    Vec2 get_minimum_size() const override;

    // --- Fit content ---------------------------------------------------------

    bool get_fit_content() const { return fit_content; }

    // Turning it on makes this Label a minimum its parents can lay out around,
    // rather than a fixed-size box that clips. Turning it off returns it to a
    // zero minimum, which lets a container give it whatever space is going.
    void set_fit_content(bool enabled) {
        if (fit_content == enabled) return;
        fit_content = enabled;
        measured_size_valid = false;
        invalidate_cache_recursively();
    }

    // --- Text ---------------------------------------------------------------

    // Re-wraps against the current content width, so a change here is visible
    // this frame without waiting for the next layout pass.
    void set_text(const std::string& text);

    const std::string& get_text() const { return raw_text; }

    // The lines as last wrapped. Derived state; read it to inspect what is
    // being drawn, but set raw_text through set_text instead.
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

    // A StyleBox change can move the content rect, so the wrap is recomputed
    // from the current text rather than left stale.
    void set_stylebox(StyleBox new_stylebox);
};

} // namespace guayaba

#endif // GUAYABA_NODES_UI_LABEL_HPP