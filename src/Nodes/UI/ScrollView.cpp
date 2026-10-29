#include <algorithm>
#include <utility>

#include <guayaba/Core/Color.hpp>
#include <guayaba/Core/Input.hpp>
#include <guayaba/Nodes/UI/ScrollView.hpp>

namespace guayaba {

namespace {

// Track and thumb glyphs, filled so the handle reads as solid. A vertical bar
// because the track is one column wide.
constexpr wchar_t TRACK_CHAR = L'│';
constexpr wchar_t HANDLE_CHAR = L'█';

} // namespace

ScrollView::ScrollView(const std::string& node_name, Vec2 node_position)
    : PanelContainer(node_name, node_position) {
    // Always on: a scroll view whose content paints past its frame is not scrolling
    // anything. Not exposed as an option.
    set_clip_children(true);

    // Defaults that read as a scrollbar: a dark rail and a light handle. Both are
    // overridable through the setters. BLACK and WHITE because there is no
    // BRIGHT_BLACK.
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

    // The handle takes as much of the track as the viewport takes of the content,
    // at least one cell tall.
    const int viewport_height = viewport_rect().size.y;
    const int content_height = content_extent().y;

    int handle_height = track.size.y;
    if (content_height > 0 && viewport_height > 0) {
        const long long scaled =
            static_cast<long long>(track.size.y) * viewport_height / content_height;
        handle_height = static_cast<int>(std::clamp<long long>(scaled, 1, track.size.y));
    }

    // Positioned by where the offset sits in the scrollable range, so the handle
    // travels the full track.
    const int travel = track.size.y - handle_height;
    const int limit = max_scroll_offset();
    const int moved = (limit > 0) ? (scroll_offset.y * travel / limit) : 0;

    return Rect{
        Vec2{track.pos.x, track.pos.y + std::clamp(moved, 0, travel)},
        Vec2{TRACK_WIDTH, handle_height},
    };
}

void ScrollView::set_scroll_offset(Vec2 new_offset) {
    // Only y scrolls. x is carried through unchanged rather than clamped, so
    // get_scroll_offset() reads back exactly what was put in.
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
    // Not PanelContainer's minimum, which would add the content's own minimum: a
    // view that could never be smaller than its content could never scroll. The
    // floor is only the frame and the track column; the height comes from the
    // size, anchors or parent.
    const Vec2 frame = stylebox.get_frame_size();
    const Vec2 own = Control::get_minimum_size();

    return {std::max(own.x, frame.x + TRACK_WIDTH), std::max(own.y, frame.y)};
}

void ScrollView::update(double delta_time) {
    // Input arrives through on_gui_input; only the visual/layout side is left here.
    PanelContainer::update(delta_time);
}

bool ScrollView::can_receive_input(const InputEvent& event) const {
    if (event.type == InputEvent::Type::KEY) return is_focused();

    // While dragging, capture motion and release wherever the pointer is, or the
    // drag would stall the moment it left the handle.
    if (dragging) {
        return event.type == InputEvent::Type::MOUSE_MOTION ||
               event.type == InputEvent::Type::MOUSE_BUTTON;
    }

    if (event.type == InputEvent::Type::MOUSE_BUTTON && event.pressed) {
        // The handle lives outside get_rect(), so is_inside would refuse it.
        return handle_rect().contains(event.mouse_pos);
    }
    if (event.type == InputEvent::Type::MOUSE_WHEEL) {
        return has_overflow() && is_inside(event.mouse_pos);
    }
    return false;
}

bool ScrollView::on_gui_input(const InputEvent& event) {
    const Rect track = track_rect();
    const Rect handle = handle_rect();

    if (event.type == InputEvent::Type::MOUSE_WHEEL) {
        if (!has_overflow()) return false;
        // Positive wheel is up, which scrolls toward the top.
        scroll_by(Vec2{0, -event.wheel_delta});
        return true;
    }

    if (event.type == InputEvent::Type::MOUSE_BUTTON) {
        if (event.pressed && event.mouse_button == MouseButton::LEFT) {
            if (!track.is_empty() && handle.contains(event.mouse_pos)) {
                // Grabbed, not jumped to: the pointer keeps the same distance from
                // the handle's top.
                dragging = true;
                grab_offset = event.mouse_pos.y - handle.pos.y;
                return true;
            }
        } else if (!event.pressed && event.mouse_button == MouseButton::LEFT &&
                   dragging) {
            // The drag ends on release wherever the pointer is; a fast drag can
            // leave the track.
            dragging = false;
            return true;
        }
    }

    if (event.type == InputEvent::Type::MOUSE_MOTION && dragging) {
        const int travel = std::max(0, track.size.y - handle.size.y);
        const int from_track_top = event.mouse_pos.y - track.pos.y - grab_offset;

        // An empty range means nothing to scroll to, so leave the offset alone.
        if (travel > 0 && max_scroll_offset() > 0) {
            const int moved = std::clamp(from_track_top, 0, travel);
            set_scroll_offset(Vec2{scroll_offset.x,
                                   static_cast<int>(static_cast<long long>(moved) *
                                                    max_scroll_offset() / travel)});
        }
        return true;
    }

    return false;
}

void ScrollView::update_layout(const Rect& parent_rect) {
    // resolve_rect then the viewport offset, not Control::update_layout: the
    // child's rect moves by the scroll offset while its size stays the content
    // rect.
    resolve_rect(parent_rect);

    // Clamped every pass: the content can grow under a fixed offset, leaving it
    // past the end.
    scroll_offset.y = std::clamp(scroll_offset.y, 0, max_scroll_offset());
    if (max_scroll_offset() == 0) dragging = false;

    const Rect viewport = viewport_rect();

    // The offset is baked into the rect handed to the child, which place_child
    // assigns outright, so nothing drifts. Only the position moves; the size is
    // the viewport's own.
    const Rect scrolled{
        Vec2{viewport.pos.x, viewport.pos.y - scroll_offset.y},
        viewport.size,
    };
    for (size_t i = 0; i < children.size(); ++i) place_child(i, scrolled);
}

void ScrollView::draw(ConsoleRenderer& renderer, const Rect& clip) {
    stylebox.apply(renderer, get_rect(), clip);

    // Drawn only when there is something to scroll; the reserved column stays
    // reserved either way, so an unscrollable view shows a blank column.
    if (has_overflow()) {
        const Rect track = track_rect();

        // The track is outside the panel's rect, so only the ancestors' clip
        // applies.
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
    // handle is drawn outside get_rect(). The viewport is get_rect() less the
    // border and margins, so the content stops at the panel's edge.
    Node::draw(renderer, viewport_rect().intersection(clip));
}

} // namespace guayaba