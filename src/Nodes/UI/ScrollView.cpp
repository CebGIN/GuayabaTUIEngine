#include <algorithm>
#include <utility>

#include <guayaba/Core/Color.hpp>
#include <guayaba/Core/Input.hpp>
#include <guayaba/Nodes/UI/ScrollView.hpp>

namespace guayaba {

namespace {

// Handle glyphs, as a track and a thumb drawn with fill_rect so the handle reads
// as solid rather than as an outline of a character cell. A vertical bar rather
// than a square, because the track is one column wide and has to.
constexpr wchar_t TRACK_CHAR = L'│';
constexpr wchar_t HANDLE_CHAR = L'█';

} // namespace

ScrollView::ScrollView(const std::string& node_name, Vec2 node_position)
    : PanelContainer(node_name, node_position) {
    // Always on, and not a preference: a scroll view whose content is allowed to
    // paint past its own frame is not scrolling anything, it is just an offset
    // with the edges left off. This is the one clip_children consumer in the tree
    // that cannot be switched off, which is why it is set here rather than
    // exposed as an option.
    set_clip_children(true);

    // Defaults that read as a scrollbar without being asked for: a dark rail and
    // a light handle, the two extremes the palette actually offers. Both are
    // overridable through the setters, and both are left without a border so a
    // caller adding one is not fighting a default.
    //
    // BLACK and WHITE rather than the bright variants because there is no
    // BRIGHT_BLACK: the 8 bright slots are bright versions of the first seven
    // plus bright white, so there is no dark grey to ask for.
    track_stylebox.set_background_color(Color::from_name(Color::BLACK));
    handle_stylebox.set_background_color(Color::from_name(Color::WHITE));
}

Rect ScrollView::viewport_rect() const {
    return panel_content_rect();
}

Vec2 ScrollView::content_extent() const {
    return Container::get_minimum_size();
}

Rect ScrollView::track_rect() const {
    const Rect box = get_rect();
    return Rect{
        Vec2{box.right(), box.top()},
        Vec2{TRACK_WIDTH, box.size.y},
    };
}

bool ScrollView::has_overflow() const {
    return content_extent().y > viewport_rect().size.y;
}

int ScrollView::max_scroll_offset() const {
    return std::max(0, content_extent().y - viewport_rect().size.y);
}

Rect ScrollView::handle_rect() const {
    const Rect track = track_rect();
    if (track.is_empty()) return track;

    // Proportional: the handle takes as much of the track as the viewport takes
    // of the content, so a view half as tall as its content has a half-height
    // handle. Rounded to at least one cell, since a handle with no height cannot
    // be seen or grabbed.
    const int viewport_height = viewport_rect().size.y;
    const int content_height = content_extent().y;

    int handle_height = track.size.y;
    if (content_height > 0 && viewport_height > 0) {
        const long long scaled =
            static_cast<long long>(track.size.y) * viewport_height / content_height;
        handle_height = static_cast<int>(std::clamp<long long>(scaled, 1, track.size.y));
    }

    // Positioned by where the offset sits in the scrollable range, so the handle
    // travels the full track as the content travels its full extent.
    const int travel = track.size.y - handle_height;
    const int limit = max_scroll_offset();
    const int moved = (limit > 0) ? (scroll_offset.y * travel / limit) : 0;

    return Rect{
        Vec2{track.pos.x, track.pos.y + std::clamp(moved, 0, travel)},
        Vec2{TRACK_WIDTH, handle_height},
    };
}

void ScrollView::set_scroll_offset(Vec2 new_offset) {
    // Horizontal scrolling is not offered: a horizontal track needs a second
    // handle and a second range, and nothing in the tree wraps in a way that
    // would want it. The x is carried through unchanged rather than clamped so
    // a caller setting a whole offset does not have to remember which half is
    // live - get_scroll_offset() reads back exactly what was put in.
    scroll_offset = Vec2{new_offset.x, std::clamp(new_offset.y, 0, max_scroll_offset())};

    if (max_scroll_offset() == 0) dragging = false;
}

void ScrollView::scroll_to_top() {
    set_scroll_offset(Vec2{scroll_offset.x, 0});
}

void ScrollView::scroll_to_bottom() {
    set_scroll_offset(Vec2{scroll_offset.x, max_scroll_offset()});
}

Vec2 ScrollView::get_minimum_size() const {
    // Deliberately NOT PanelContainer's minimum, which adds the content's own
    // minimum to the frame. That is right for a panel - a panel is sized by what
    // is in it - and exactly wrong here: a view whose minimum included its
    // content could never be smaller than its content, so it could never have
    // anything to scroll to, and it would grow to fit instead. The whole point
    // of the node is that the content is taller than the view.
    //
    // So the floor is only the frame and the track column. What the view is
    // actually tall comes from its size, its anchors, or whatever the parent
    // imposes - and the content overflows that on purpose.
    const Vec2 frame = stylebox.get_frame_size();
    const Vec2 own = Control::get_minimum_size();

    return {std::max(own.x, frame.x + TRACK_WIDTH), std::max(own.y, frame.y)};
}

void ScrollView::update(double delta_time) {
    // Everything below hit-tests the rects resolved last frame, which are also
    // the ones this frame draws. Layout runs between update() and draw(), so using
    // the freshest rect here would mean reacting to a size the user has not seen.
    const Rect track = track_rect();
    const Rect handle = handle_rect();

    if (!dragging && Input::l_click_just_pressed && !track.is_empty()) {
        if (handle.contains(Input::mouse_pos)) {
            // Grabbed, not jumped to: the pointer stays the same distance from
            // the handle's top, so the content under the cursor keeps the cursor.
            dragging = true;
            grab_offset = Input::mouse_pos.y - handle.pos.y;
        }
    }

    if (dragging) {
        // The drag ends on release wherever the pointer is, rather than by
        // hit-testing the track: a fast drag can leave the track entirely, and
        // dropping the handle then would be the one case the handle cannot be
        // put back. set_scroll_offset clamps, so the resting place is legal.
        if (!Input::l_click) {
            dragging = false;
        } else {
            const int travel = std::max(0, track.size.y - handle.size.y);
            const int from_track_top = Input::mouse_pos.y - track.pos.y - grab_offset;

            // Empty range means there is nothing to scroll to, so the offset is
            // left alone rather than divided by zero or snapped to the end.
            if (travel > 0 && max_scroll_offset() > 0) {
                const int moved = std::clamp(from_track_top, 0, travel);
                set_scroll_offset(Vec2{scroll_offset.x,
                                       static_cast<int>(static_cast<long long>(moved) *
                                                        max_scroll_offset() / travel)});
            }
        }
    }

    PanelContainer::update(delta_time);
}

void ScrollView::update_layout(const Rect& parent_rect) {
    // resolve_rect and then the viewport offset - never Control::update_layout -
    // because the child's rect has to move by the scroll offset while its size
    // stays the panel's content rect.
    resolve_rect(parent_rect);

    // Clamped every pass, not only when someone calls set_scroll_offset, because
    // the content can grow under a fixed offset: a Label set to more lines, or a
    // child added to a VBox, both make a resting offset no longer the end.
    scroll_offset.y = std::clamp(scroll_offset.y, 0, max_scroll_offset());
    if (max_scroll_offset() == 0) dragging = false;

    const Rect viewport = viewport_rect();

    // The scroll offset is baked into the rect handed to the child, and
    // place_child assigns that rect's position outright every pass rather than
    // shifting by a delta. That is what keeps a scroll view from drifting: there
    // is no running total to forget to take back, so a change of offset, a
    // reorder or a resize all leave the child exactly where this put it.
    //
    // Only the position moves. The size is the viewport's own, which is what the
    // child fills - so a Label keeps wrapping to the width it can actually show
    // while its top edge slides up out of sight.
    const Rect scrolled{
        Vec2{viewport.pos.x, viewport.pos.y - scroll_offset.y},
        viewport.size,
    };
    for (size_t i = 0; i < children.size(); ++i) place_child(i, scrolled);
}

void ScrollView::draw(ConsoleRenderer& renderer, const Rect& clip) {
    stylebox.apply(renderer, get_rect(), clip);

    // Drawn only when there is something to scroll. The reserved column stays
    // reserved either way, since get_minimum_size cannot ask the content how tall
    // it is, so an unscrollable view shows a blank column rather than a handle
    // that goes nowhere.
    if (has_overflow()) {
        const Rect track = track_rect();

        // The track is outside the panel's own rect, so it is clipped by the
        // ancestors' clip only. Narrowing it to the frame would cut it off.
        if (track_stylebox.has_background() || track_stylebox.has_border()) {
            track_stylebox.apply(renderer, track, clip);
        } else {
            renderer.fill_rect(track, TRACK_CHAR, /*fg=*/0, /*bg=*/0, clip);
        }

        const Rect handle = handle_rect();
        if (!handle.is_empty() &&
            (handle_stylebox.has_background() || handle_stylebox.has_border())) {
            handle_stylebox.apply(renderer, handle, clip);
        } else {
            renderer.fill_rect(handle, HANDLE_CHAR, /*fg=*/0, /*bg=*/0, clip);
        }
    }

    // Handed the viewport rather than letting Control narrow to get_rect(): the
    // handle is drawn outside get_rect(), and Control's narrowing is what decides
    // what the children may paint over. The viewport is the stricter of the two -
    // it is get_rect() less the border and margins - so the content stops at the
    // panel's edge while the track, which lives out there, is already drawn.
    Node::draw(renderer, viewport_rect().intersection(clip));
}

} // namespace guayaba