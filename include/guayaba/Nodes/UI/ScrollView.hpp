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

    It is a PanelContainer rather than a Container because a scroll view has to
    fill its frame the way a panel does - the point is that the content keeps
    going past the edge, which only means something if the edge is where it says
    it is. So the panel draws, the content rect is the panel's content rect, and
    the child fills it exactly as a PanelContainer's child would.

    THE SCROLLBAR GOES OUTSIDE THE FRAME

    The track is one column to the right of the panel's own rect rather than
    inside it, and get_minimum_size() reserves it the same way
    PanelContainer::get_minimum_size() reserves the border and margins: it is
    added to the frame, not centred inside the content.

    WHY THE MINIMUM SIZE IGNORES THE CONTENT

    PanelContainer's minimum size adds what its child needs, so a panel is as
    big as its content. A ScrollView deliberately does not, and this is the one
    place it parts company with its parent class:

    A view sized to its content can never be smaller than its content, so it
    could never have anything to scroll to, and it would grow to fit instead of
    scrolling. The whole point of the node is that the content is taller than
    the view is.

    So the minimum is only the frame and the track column. The view is as tall as
    its size, its anchors or its parent make it, and the content overflows that
    deliberately. The content is measured anyway - see content_extent() - but only
    to know how far it can be scrolled, never to decide how big the view is.

    THE CHILD HAS TO FILL THE VIEWPORT

    As with a PanelContainer, the child fills the content rect only if it is told
    to - Grow::Both, or the FullRect preset. A child left at its default anchors
    to its own top-left and keeps whatever size it asked for, which inside a
    scroll view almost always means a collapsed box at the corner of the content.

    And the content has to report a size for any of this to matter: a Control whose
    get_minimum_size() is zero contributes nothing to content_extent(), so a
    ScrollView full of Labels that do not set_fit_content(true) has nothing to
    scroll, however much text they hold.

    WHEN IT APPEARS

    The track and handle are drawn only while the content is taller than the
    viewport. The reserved column stays reserved either way - the minimum size
    cannot depend on whether the content overflows without measuring the content
    to decide, and that measurement is only available after layout has run - but
    when there is nothing to scroll the column is simply left blank rather than
    showing a handle that does nothing.

    WHY A HANDLE AND NOT A WHEEL

    The engine has no wheel input: IOSTerminal exposes mouse position and the two
    click buttons, and nothing else. Rather than fake a wheel, this exposes an
    ordinary draggable handle, which the terminal can answer with the click input
    it already has. Wheel support is still the better input for a list and is
    still missing; when it arrives, scrolling becomes a second path into the same
    set_scroll_offset() that dragging already uses.

    HOW THE DRAG WORKS

    Pressing inside the handle starts a drag and remembers where in the handle the
    pointer was, so the handle does not jump to centre itself under the cursor.
    While the button is held, the pointer's distance from the top of the track is
    mapped across the content extent. Releasing ends the drag, wherever the
    pointer is - the offset is clamped by the layout pass, so a drag released
    past the end rests at the end.

    The handle is only grabbed on a press, so it is never scrolled by the pointer
    merely passing over it, and a drag that starts on the handle cannot be stolen
    by a ScrollView behind it.

    Hit testing uses the rects resolved last frame, which is also what this frame
    draws, the same trade Button::update makes: layout runs between update() and
    draw(), so hit-testing the freshest rect would mean acting on a size the user
    has not seen yet.
*/
class ScrollView : public PanelContainer {
private:
    // Cells reserved for the track. One column is the narrowest thing that still
    // reads as a scrollbar, and a wider one is a StyleBox border away.
    static constexpr int TRACK_WIDTH = 1;

    Vec2 scroll_offset = {0, 0};

    bool dragging = false;

    // Pointer distance from the top of the handle at the moment it was grabbed.
    // Remembered rather than recomputed so a drag does not snap the handle to
    // centre itself on the first frame of movement.
    int grab_offset = 0;

    StyleBox track_stylebox;
    StyleBox handle_stylebox;

    // The area the child fills: the panel's content rect. Independent of the
    // scrollbar, which sits outside the frame and takes no width from it.
    Rect viewport_rect() const;

    // How tall the content is, which is not the same as how tall this view is.
    //
    // Read from Container::get_minimum_size() - the largest minimum among the
    // Control children - rather than from Container::content_rect(). The latter is
    // sized to get_minimum_size(), which for a ScrollView includes the frame and
    // the track column, so it would measure the panel against itself and report
    // no overflow ever. This measures what is actually inside.
    //
    // It is the intrinsic extent, available before layout runs, which is what
    // lets the offset be clamped in the same pass that moves the child.
    Vec2 content_extent() const;

    // The column to the right of the panel's own rect, full height.
    Rect track_rect() const;

    // Where the handle sits for the current offset, sized so its share of the
    // track matches the viewport's share of the content. Empty when there is
    // nothing to scroll, and never taller than the track.
    Rect handle_rect() const;

    // How far the content can travel: content height minus viewport height,
    // never negative.
    int max_scroll_offset() const;

    // True when the content is taller than the viewport, which is what decides
    // whether the scrollbar is drawn and whether the offset can be anything but
    // zero.
    bool has_overflow() const;

public:
    explicit ScrollView(const std::string& node_name = "Unnamed ScrollView",
                        Vec2 node_position = {0, 0});

    void update(double delta_time) override;

    void update_layout(const Rect& parent_rect) override;

    Vec2 get_minimum_size() const override;

    void draw(ConsoleRenderer& renderer, const Rect& clip) override;

    // --- Scrolling ------------------------------------------------------------

    Vec2 get_scroll_offset() const { return scroll_offset; }

    // Clamped against the layout resolved so far, so the offset cannot leave the
    // content somewhere unreachable. A view with nothing to scroll sits at zero.
    void set_scroll_offset(Vec2 new_offset);

    // Moves by a delta, which is clamped the same way. Negative y scrolls back
    // towards the top, the way a wheel and a dragged handle both move.
    void scroll_by(Vec2 delta) { set_scroll_offset(scroll_offset + delta); }

    // Absolute positions, so a caller does not have to know the extent to use
    // them. Each clamps; scrolling to the top of content that cannot scroll
    // leaves it at the top, which is the same place it already was.
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