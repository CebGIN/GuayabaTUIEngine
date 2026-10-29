#include <algorithm>
#include <memory>

#include <guayaba/Nodes/UI/MarginContainer.hpp>

namespace guayaba {

MarginContainer::MarginContainer(const std::string& node_name, Vec2 node_position)
    : Container(node_name, node_position) {}

void MarginContainer::set_margins(EdgeSize new_margins) {
    margins.left = std::max(0, new_margins.left);
    margins.top = std::max(0, new_margins.top);
    margins.right = std::max(0, new_margins.right);
    margins.bottom = std::max(0, new_margins.bottom);
}

void MarginContainer::set_margins(int left, int top, int right, int bottom) {
    set_margins(EdgeSize{left, top, right, bottom});
}

Rect MarginContainer::margin_content_rect() const {
    const Rect box = get_rect();
    return Rect{
        Vec2{box.pos.x + margins.left, box.pos.y + margins.top},
        Vec2{std::max(0, box.size.x - margins.horizontal()),
             std::max(0, box.size.y - margins.vertical())},
    };
}

void MarginContainer::on_update_layout(const Rect& parent_rect) {
    resolve_rect(parent_rect);

    const Rect content = margin_content_rect();
    for (size_t i = 0; i < children.size(); ++i) place_child(i, content);
}

Vec2 MarginContainer::get_minimum_size() const {
    // The largest child minimum, as in a plain Container: several children share
    // the same content rect instead of stacking.
    Vec2 content{0, 0};
    for (const std::unique_ptr<Node>& child : children) {
        if (!child->is_visible()) continue;

        const Control* as_control = dynamic_cast<const Control*>(child.get());
        if (as_control == nullptr) continue;

        const Vec2 child_minimum = as_control->get_minimum_size();
        content.x = std::max(content.x, child_minimum.x);
        content.y = std::max(content.y, child_minimum.y);
    }

    const Vec2 own = Control::get_minimum_size();
    return {
        std::max(own.x, content.x + margins.horizontal()),
        std::max(own.y, content.y + margins.vertical()),
    };
}

} // namespace guayaba
