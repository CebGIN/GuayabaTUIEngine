#ifndef GUAYABA_NODES_UI_SCROLLVIEW_HPP
#define GUAYABA_NODES_UI_SCROLLVIEW_HPP

#include <string>

#include <guayaba/Core/Input.hpp>
#include <guayaba/Core/Rect.hpp>
#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/UI/PanelContainer.hpp>

namespace guayaba {

/*
    A PanelContainer whose content can be scrolled by dragging a handle.

        +---------------------------+--+-+
        | | first line              | |#| |
        | | second line             | |#| |  <- track
        | | third line              | |#| |
        | | fourth line             |#| | |
        | | fifth line              |#| | |  <- handle, drag me
        +---------------------------+--+-+

    It is a PanelContainer so the frame draws and the child fills the content rect
    exactly as a panel's child would.

    The track is one column to the right of the panel's rect, not inside it, and
    get_minimum_size() reserves it alongside the frame. The minimum size does not
    include the content: a view sized to its content could never overflow and so
    could never scroll. Content is measured only to know how far it can scroll
    (content_extent()).

    The child fills the viewport only if told to (Grow::Both or FullRect), and it
    must report a size - a Control with a zero minimum contributes nothing to
    content_extent(), so Labels need fit_content(true) to be scrollable.

    The track and handle are drawn only while the content overflows; the reserved
    column stays reserved either way and is left blank when there is nothing to
    scroll. Pressing the handle starts a drag mapped across the content extent; the
    wheel scrolls too and both funnel into set_scroll_offset(). While dragging, the
    view captures mouse events wherever the pointer is (can_receive_input).

    Hit testing uses the rects resolved last frame, which is also what this frame
    draws, since layout runs between update() and draw().
*/
class ScrollView : public PanelContainer {
private:
    // Cells reserved for the track; one column is the narrowest scrollbar.
    static constexpr int TRACK_WIDTH = 1;

    Vec2 scroll_offset = {0, 0};

    bool dragging = false;

    // Pointer distance from the top of the handle when it was grabbed, so a drag
    // does not snap the handle to centre.
    int grab_offset = 0;

    StyleBox track_stylebox;
    StyleBox handle_stylebox;

    // The area the child fills: the panel's content rect, independent of the scrollbar.
    Rect viewport_rect() const;

    // The intrinsic extent of the content, from Container::get_minimum_size(). Not
    // Container::content_rect(), which for a ScrollView includes the frame and track
    // and would never report overflow. Available before layout runs.
    Vec2 content_extent() const;

    // The column to the right of the panel's own rect, full height.
    Rect track_rect() const;

    // Where the handle sits for the current offset, sized to the viewport's share
    // of the content. Empty with nothing to scroll.
    Rect handle_rect() const;

    // Content height minus viewport height, never negative.
    int max_scroll_offset() const;

    // True when the content is taller than the viewport.
    bool has_overflow() const;

public:
    explicit ScrollView(const std::string& node_name = "Unnamed ScrollView",
                        Vec2 node_position = {0, 0});

    void update(double delta_time) override;

    void update_layout(const Rect& parent_rect) override;

    // A press on the handle starts the drag; while dragging this view captures
    // motion and release wherever the pointer is.
    bool can_receive_input(const InputEvent& event) const override;
    bool on_gui_input(const InputEvent& event) override;

    Vec2 get_minimum_size() const override;

    void draw(ConsoleRenderer& renderer, const Rect& clip) override;

    // --- Scrolling ------------------------------------------------------------

    Vec2 get_scroll_offset() const { return scroll_offset; }

    // Clamped against the layout resolved so far. A view with nothing to scroll sits at zero.
    void set_scroll_offset(Vec2 new_offset);

    // Moves by a delta, clamped the same way. Negative y scrolls towards the top.
    void scroll_by(Vec2 delta) { set_scroll_offset(scroll_offset + delta); }

    // Absolute positions, each clamped.
    void scroll_to_top();
    void scroll_to_bottom();

    bool is_dragging() const { return dragging; }

    // --- Appearance -----------------------------------------------------------

    StyleBox& get_track_stylebox() { return track_stylebox; }
    const StyleBox& get_track_stylebox() const { return track_stylebox; }
    void set_track_stylebox(StyleBox new_stylebox) { track_stylebox = std::move(new_stylebox); }

    StyleBox& get_handle_stylebox() { return handle_stylebox; }
    const StyleBox& get_handle_stylebox() const { return handle_stylebox; }
    void set_handle_stylebox(StyleBox new_stylebox) { handle_stylebox = std::move(new_stylebox); }
};

} // namespace guayaba

#endif // GUAYABA_NODES_UI_SCROLLVIEW_HPP