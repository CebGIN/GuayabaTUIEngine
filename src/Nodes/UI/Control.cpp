#include <algorithm>
#include <cmath>

#include <guayaba/Core/Focus.hpp>
#include <guayaba/Core/NodeRef.hpp>
#include <guayaba/Core/NodeRegistry.hpp>
#include <guayaba/Core/Script.hpp>
#include <guayaba/Nodes/UI/Control.hpp>

namespace guayaba {

namespace {

// Where a named preset sits, as (left, right, top, bottom) ratios.
struct AnchorRatios {
    float left, right, top, bottom;
};

AnchorRatios ratios_of(AnchorPreset preset) {
    switch (preset) {
        case AnchorPreset::TopLeft:     return {0.0f, 0.0f, 0.0f, 0.0f};
        case AnchorPreset::Top:         return {0.5f, 0.5f, 0.0f, 0.0f};
        case AnchorPreset::TopRight:    return {1.0f, 1.0f, 0.0f, 0.0f};
        case AnchorPreset::Center:      return {0.5f, 0.5f, 0.5f, 0.5f};
        case AnchorPreset::Left:        return {0.0f, 0.0f, 0.5f, 0.5f};
        case AnchorPreset::Right:       return {1.0f, 1.0f, 0.5f, 0.5f};
        case AnchorPreset::BottomLeft:  return {0.0f, 0.0f, 1.0f, 1.0f};
        case AnchorPreset::Bottom:      return {0.5f, 0.5f, 1.0f, 1.0f};
        case AnchorPreset::BottomRight: return {1.0f, 1.0f, 1.0f, 1.0f};
        case AnchorPreset::FullRect:    return {0.0f, 1.0f, 0.0f, 1.0f};
    }
    return {0.0f, 0.0f, 0.0f, 0.0f};
}

// Exact float equality, on purpose: get_anchor_preset() reports the preset a
// control actually has, and a preset written back must round-trip to itself.
bool same(float a, float b) { return a == b; }

// Keeps right >= left so an inverted pair of anchors cannot produce a negative
// extent, which would flip the node inside out instead of clamping it.
float span(float start, float end) {
    return std::max(0.0f, end - start);
}

} // namespace

Control::Control(const std::string& node_name, Vec2 node_position)
    : Node2D(node_name) {
    set_local_position(node_position);
}

// --- Layout -----------------------------------------------------------------

void Control::update_layout(const Rect& parent_rect) {
    resolve_rect(parent_rect);
    Node::update_layout(cached_rect);
}

void Control::resolve_rect(const Rect& parent_rect) {
    const int parent_width = parent_rect.size.x;
    const int parent_height = parent_rect.size.y;

    // The slice of the parent rect these anchors name. Anchors past 1 or below
    // 0 are not clamped: pushing a control off its parent's edge is a normal
    // way to hide part of it, and the parent rect is already screen-bounded.
    const float anchor_rect_x = static_cast<float>(parent_width) * anchor_left;
    const float anchor_rect_y = static_cast<float>(parent_height) * anchor_top;
    const float anchor_rect_w =
        static_cast<float>(parent_width) * span(anchor_left, anchor_right);
    const float anchor_rect_h =
        static_cast<float>(parent_height) * span(anchor_top, anchor_bottom);

    // Grow::Both fills the anchor rect; every other mode keeps the size the node
    // asked for. Either way a floor applies, which is what makes Grow::None mean
    // "stay this big" instead of "collapse to nothing".
    int resolved_w = (grow_x == Grow::Both) ? std::max(0, static_cast<int>(std::lround(anchor_rect_w)))
                                           : size.x;
    int resolved_h = (grow_y == Grow::Both) ? std::max(0, static_cast<int>(std::lround(anchor_rect_h)))
                                           : size.y;

    // The floor: whichever of the content-derived and caller-imposed minimums is
    // larger, so neither can shrink the other.
    const Vec2 floor = get_minimum_size();
    resolved_w = std::max(resolved_w, floor.x);
    resolved_h = std::max(resolved_h, floor.y);

    // How far into the anchor rect the node sits, in pixels.
    //
    // With Grow::None the anchors themselves decide, via the start ratio: 0 pins
    // the node to the near edge, 0.5 centres it, 1 pins it to the far edge. That
    // is what makes the point presets work at all - their anchor rect is degenerate
// - and it is why TopLeft pins a corner while Center sits on its point.
    auto align = [](int extent, int resolved, Grow grow, float start_ratio) {
        const float fraction = (grow == Grow::None) ? start_ratio
                              : (grow == Grow::End)  ? 1.0f
                              : (grow == Grow::Begin) ? 0.0f
                                                     : 0.0f;
        // floor, not round: a half-cell centre rounds either way, and the floor
        // keeps the node biased toward the start edge, which is what a centred
        // odd-sized run of text should do.
        return static_cast<int>(std::floor((extent - resolved) * fraction));
    };

    int pos_x = static_cast<int>(std::lround(anchor_rect_x)) +
                align(static_cast<int>(std::lround(anchor_rect_w)), resolved_w, grow_x, anchor_left);
    int pos_y = static_cast<int>(std::lround(anchor_rect_y)) +
                align(static_cast<int>(std::lround(anchor_rect_h)), resolved_h, grow_y, anchor_top);

    const Rect local{Vec2{pos_x, pos_y}, Vec2{resolved_w, resolved_h}};
    cached_rect = local.translated(get_global_position());
}

void Control::invalidate_cache_recursively() {
    cached_rect = Rect{};
    Node2D::invalidate_cache_recursively();
}

// --- Anchors ----------------------------------------------------------------

AnchorPreset Control::get_anchor_preset() const {
    static const AnchorPreset all[] = {
        AnchorPreset::TopLeft,     AnchorPreset::Top,     AnchorPreset::TopRight,
        AnchorPreset::Center,      AnchorPreset::Left,    AnchorPreset::Right,
        AnchorPreset::BottomLeft,  AnchorPreset::Bottom,  AnchorPreset::BottomRight,
        AnchorPreset::FullRect,
    };

    for (AnchorPreset preset : all) {
        const AnchorRatios r = ratios_of(preset);
        if (same(anchor_left, r.left) && same(anchor_right, r.right) &&
            same(anchor_top, r.top) && same(anchor_bottom, r.bottom)) {
            return preset;
        }
    }
    // Not a named combination - the ratios are still valid, just not a preset.
    return AnchorPreset::TopLeft;
}

void Control::set_anchor_preset(AnchorPreset preset) {
    const AnchorRatios r = ratios_of(preset);
    anchor_left = r.left;
    anchor_right = r.right;
    anchor_top = r.top;
    anchor_bottom = r.bottom;

    // A preset that spans an area has to stretch, or FullRect would name a
    // 0x0 node. The point presets keep Grow::None, which reads their alignment
    // off the ratios just set.
    if (preset == AnchorPreset::FullRect) {
        grow_x = Grow::Both;
        grow_y = Grow::Both;
    } else {
        grow_x = Grow::None;
        grow_y = Grow::None;
    }
}

void Control::set_anchor_left(float ratio) { anchor_left = ratio; }
void Control::set_anchor_right(float ratio) { anchor_right = ratio; }
void Control::set_anchor_top(float ratio) { anchor_top = ratio; }
void Control::set_anchor_bottom(float ratio) { anchor_bottom = ratio; }

// --- Size -------------------------------------------------------------------

void Control::set_minimum_size(Vec2 new_minimum_size) {
    minimum_size = {std::max(0, new_minimum_size.x), std::max(0, new_minimum_size.y)};
}

Vec2 Control::get_minimum_size() const {
    return {std::max(minimum_size.x, custom_minimum_size.x),
            std::max(minimum_size.y, custom_minimum_size.y)};
}

void Control::set_custom_minimum_size(Vec2 new_custom_minimum_size) {
    custom_minimum_size = {std::max(0, new_custom_minimum_size.x),
                           std::max(0, new_custom_minimum_size.y)};
}

void Control::set_size(Vec2 new_size) {
    size = {std::max(0, new_size.x), std::max(0, new_size.y)};
}

void Control::set_grow_x(Grow grow) { grow_x = grow; }
void Control::set_grow_y(Grow grow) { grow_y = grow; }

// --- Geometry ---------------------------------------------------------------

bool Control::is_inside(Vec2 point) const {
    return cached_rect.contains(point);
}

// --- Clipping ---------------------------------------------------------------

void Control::set_clip_children(bool enabled) {
    clip_children = enabled;
}

Rect Control::get_child_clip(const Rect& incoming_clip) const {
    if (!clip_children) return incoming_clip;
    return incoming_clip.intersection(cached_rect);
}

void Control::draw(ConsoleRenderer& renderer, const Rect& clip) {
    // The narrowing is computed here and handed to Node::draw, which passes it
    // to every child. Our own drawing keeps the clip we were given: clip_children
    // governs what this node contains, not whether the node itself is visible.
    const Rect child_clip = get_child_clip(clip);
    Node::draw(renderer, child_clip);
}

// --- Input ------------------------------------------------------------------

void Control::route_input(std::vector<InputEvent>& events) {
    const uint64_t my_id = instance_id();
    if (!detail::NodeRegistry::instance().is_live(my_id)) return;

    // Children in reverse: a later sibling draws on top, so it gets first refusal.
    // The reverse walk is done here rather than by calling Node::route_input, whose
    // forward walk would visit every subtree twice.
    for (size_t i = children.size(); i > 0; --i) {
        if (i - 1 >= children.size()) continue;
        children[i - 1]->route_input(events);

        if (!detail::NodeRegistry::instance().is_live(my_id)) return;
        if (events.empty()) return;
    }

    // This control's turn, over every event the children left. An event it
    // consumes is erased; one it declines stays for whoever comes next.
    for (size_t i = 0; i < events.size();) {
        if (!can_receive_input(events[i])) {
            ++i;
            continue;
        }
        if (handle_input(events[i])) {
            events.erase(events.begin() + static_cast<std::ptrdiff_t>(i));
            // Do not advance: the next event shifted into this slot.
        } else {
            ++i;
        }
    }

    // The script gets what the control's own gui handling left.
    if (script) {
        for (size_t i = 0; i < events.size();) {
            if (script->on_input(events[i])) {
                events.erase(events.begin() + static_cast<std::ptrdiff_t>(i));
            } else {
                ++i;
            }
        }
    }
}

bool Control::can_receive_input(const InputEvent& event) const {
    switch (event.type) {
        case InputEvent::Type::MOUSE_MOTION:
        case InputEvent::Type::MOUSE_BUTTON:
        case InputEvent::Type::MOUSE_WHEEL:
            return is_inside(event.mouse_pos);

        case InputEvent::Type::KEY:
            // Keys go to the focused control and to nothing else.
            return is_focused();
    }
    return false;
}

bool Control::handle_input(const InputEvent& event) {
    if (!can_receive_input(event)) return false;

    // Announced before the behaviour, so a listener hears the event whether or
    // not on_gui_input consumes it.
    emit_signal("gui_input", event);

    return on_gui_input(event);
}

bool Control::on_gui_input(const InputEvent& event) {
    (void)event;
    return false;
}

// --- Focus ------------------------------------------------------------------

void Control::grab_focus() {
    if (focus_mode == FocusMode::NONE) return;
    FocusManager::get_instance().set_focused(this);
}

void Control::release_focus() {
    if (!focused) return;
    FocusManager::get_instance().set_focused(nullptr);
}

void Control::set_focused_internal(bool now_focused) {
    if (focused == now_focused) return;
    focused = now_focused;
    emit_signal(now_focused ? "focus_entered" : "focus_exited");
}

} // namespace guayaba