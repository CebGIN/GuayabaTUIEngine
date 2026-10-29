#include <algorithm>
#include <cmath>

#include <guayaba/Nodes/UI/SplitContainer.hpp>

namespace guayaba {

SplitContainer::SplitContainer(const std::string& node_name, Vec2 node_position)
    : Container(node_name, node_position) {
    declare_signal<float>("split_ratio_changed");

    // A click on the divider gives it the focus, so the arrows work afterwards.
    set_focus_mode(FocusMode::CLICK);
}

void SplitContainer::set_split_ratio(float new_ratio) {
    const float clamped = std::clamp(new_ratio, 0.0f, 1.0f);
    if (clamped == split_ratio) return;

    split_ratio = clamped;
    emit_signal("split_ratio_changed", split_ratio);
}

void SplitContainer::set_min_first(int cells) {
    min_first = std::max(0, cells);
}

void SplitContainer::set_min_second(int cells) {
    min_second = std::max(0, cells);
}

void SplitContainer::set_divider_width(int cells) {
    divider_width = std::max(0, cells);
}

std::vector<Control*> SplitContainer::panels() const {
    std::vector<Control*> out;
    for (const std::unique_ptr<Node>& child : children) {
        if (!child->is_visible()) continue;

        Control* control = dynamic_cast<Control*>(child.get());
        if (control == nullptr) continue;

        out.push_back(control);
        if (out.size() == 2) break;
    }
    return out;
}

int SplitContainer::axis_extent(const Rect& box) const {
    return orientation == SplitOrientation::VerticalDivider ? box.size.x : box.size.y;
}

int SplitContainer::axis_coord(Vec2 point) const {
    return orientation == SplitOrientation::VerticalDivider ? point.x : point.y;
}

int SplitContainer::free_extent() const {
    return std::max(0, axis_extent(get_rect()) - divider_width - min_first - min_second);
}

int SplitContainer::first_extent() const {
    const int free = free_extent();
    const int available = std::max(0, axis_extent(get_rect()) - divider_width);
    const int first = min_first + static_cast<int>(std::lround(split_ratio * free));

    return std::clamp(first, 0, available);
}

Rect SplitContainer::divider_rect() const {
    const Rect box = get_rect();
    const int first = first_extent();
    const int divider = std::min(divider_width, std::max(0, axis_extent(box) - first));

    if (orientation == SplitOrientation::VerticalDivider) {
        return Rect{Vec2{box.pos.x + first, box.pos.y}, Vec2{divider, box.size.y}};
    }
    return Rect{Vec2{box.pos.x, box.pos.y + first}, Vec2{box.size.x, divider}};
}

Vec2 SplitContainer::get_minimum_size() const {
    int cross = 0;
    for (Control* panel : panels()) {
        const Vec2 minimum = panel->get_minimum_size();
        cross = std::max(cross,
                         orientation == SplitOrientation::VerticalDivider ? minimum.y
                                                                          : minimum.x);
    }

    const int axis = min_first + min_second + divider_width;
    const Vec2 result = orientation == SplitOrientation::VerticalDivider
                            ? Vec2{axis, cross}
                            : Vec2{cross, axis};

    const Vec2 own = Control::get_minimum_size();
    return {std::max(own.x, result.x), std::max(own.y, result.y)};
}

void SplitContainer::on_update_layout(const Rect& parent_rect) {
    resolve_rect(parent_rect);

    // Fewer than two panels: nothing to split and no divider to place.
    if (panels().size() < 2) return;

    const Rect box = get_rect();
    const int total = axis_extent(box);
    const int first = first_extent();
    const int divider = std::min(divider_width, std::max(0, total - first));
    const int second = std::max(0, total - first - divider);

    Rect first_rect;
    Rect second_rect;
    if (orientation == SplitOrientation::VerticalDivider) {
        first_rect = Rect{Vec2{box.pos.x, box.pos.y}, Vec2{first, box.size.y}};
        second_rect = Rect{Vec2{box.pos.x + first + divider, box.pos.y},
                           Vec2{second, box.size.y}};
    } else {
        first_rect = Rect{Vec2{box.pos.x, box.pos.y}, Vec2{box.size.x, first}};
        second_rect = Rect{Vec2{box.pos.x, box.pos.y + first + divider},
                           Vec2{box.size.x, second}};
    }

    size_t placed = 0;
    for (size_t i = 0; i < children.size(); ++i) {
        Control* control = dynamic_cast<Control*>(children[i].get());
        if (control == nullptr || !control->is_visible()) continue;

        // Extra Control children are ignored, not placed.
        if (placed == 0) {
            place_child(i, first_rect);
        } else if (placed == 1) {
            place_child(i, second_rect);
        } else {
            break;
        }
        ++placed;
    }
}

void SplitContainer::on_draw(ConsoleRenderer& renderer, const Rect& clip) {
    if (panels().size() >= 2) {
        // An empty StyleBox paints nothing, which is the default: the divider is
        // invisible but still hit-tested.
        divider_stylebox.apply(renderer, divider_rect(), clip);
    }

    Control::on_draw(renderer, clip);
}

bool SplitContainer::on_gui_input(const InputEvent& event) {
    if (event.type == InputEvent::Type::MOUSE_BUTTON) {
        if (event.pressed && event.mouse_button == MouseButton::LEFT) {
            if (!divider_rect().contains(event.mouse_pos)) return false;

            grab_focus();
            capture_mouse();
            if (!has_mouse_capture()) return true;

            dragging = true;
            drag_axis = axis_coord(event.mouse_pos);
            return true;
        }

        if (!event.pressed && event.mouse_button == MouseButton::LEFT && dragging) {
            release_mouse();
            dragging = false;
            return true;
        }

        return false;
    }

    if (event.type == InputEvent::Type::MOUSE_MOTION && dragging) {
        // The capture can be gone if the control was hidden mid-drag.
        if (!has_mouse_capture()) {
            dragging = false;
            return false;
        }

        const int current = axis_coord(event.mouse_pos);
        const int delta = current - drag_axis;
        drag_axis = current;

        const int free = free_extent();
        if (delta != 0 && free > 0) {
            set_split_ratio(split_ratio + static_cast<float>(delta) / static_cast<float>(free));
        }
        return true;
    }

    if (event.type == InputEvent::Type::KEY && event.pressed) {
        int direction = 0;
        if (orientation == SplitOrientation::VerticalDivider) {
            if (event.key == Key::LEFT) direction = -1;
            else if (event.key == Key::RIGHT) direction = 1;
        } else {
            if (event.key == Key::UP) direction = -1;
            else if (event.key == Key::DOWN) direction = 1;
        }

        if (direction == 0) return false;

        const int free = free_extent();
        if (free <= 0) return true;

        // Derived from the effective extent, so one press is exactly one cell
        // even at a rounding boundary.
        const int current = first_extent();
        const int next = std::clamp(current + direction, min_first, min_first + free);
        if (next != current) {
            set_split_ratio(static_cast<float>(next - min_first) / static_cast<float>(free));
        }
        return true;
    }

    return false;
}

} // namespace guayaba
