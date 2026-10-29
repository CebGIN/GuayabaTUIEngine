#include <algorithm>

#include <guayaba/Nodes/UI/Container.hpp>

namespace guayaba {

Container::Container(const std::string& node_name, Vec2 node_position)
    : Control(node_name, node_position) {}

void Container::on_update_layout(const Rect& parent_rect) {
    // resolve_rect, not Control::on_update_layout: the base would hand our own rect
    // down and then we would lay the children out twice.
    resolve_rect(parent_rect);

    // Own rect first: content_rect() reads get_rect().
    const Rect content = content_rect();
    for (size_t i = 0; i < children.size(); ++i) place_child(i, content);
}

void Container::place_child(size_t index, const Rect& rect) {
    Node* child = children[index].get();

    // A hidden child keeps its place in the tree but takes no room and is not
    // laid out; showing it again lays it out on the next pass.
    if (!child->is_visible()) return;

    // Every positioned child is placed, not only the Controls: a NodeTextMatrix is
    // laid out against the same rect. A plain Node has no position and is skipped.
    if (Node2D* positioned = dynamic_cast<Node2D*>(child)) {
        positioned->set_local_position(rect.pos - get_global_position());
    }

    child->update_layout(rect);
}

Vec2 Container::get_minimum_size() const {
    const Vec2 own = Control::get_minimum_size();

    // The largest minimum size among the Control children. Read from their state,
    // not their resolved rects, so this can run inside Control::resolve_rect
    // without a cycle.
    Vec2 content{0, 0};
    for (const std::unique_ptr<Node>& child : children) {
        if (!child->is_visible()) continue;

        const Control* as_control = dynamic_cast<const Control*>(child.get());
        if (as_control == nullptr) continue;

        const Vec2 child_minimum = as_control->get_minimum_size();
        content.x = std::max(content.x, child_minimum.x);
        content.y = std::max(content.y, child_minimum.y);
    }

    // The same floor Control applies between its own minimums: a Container forced
    // bigger than its children pads the block instead of collapsing to it.
    return {std::max(own.x, content.x), std::max(own.y, content.y)};
}

Rect Container::content_rect() const {
    const Rect box = get_rect();

    // resolve_rect already floored the resolved size at this minimum, so the
    // block cannot overflow us.
    const Vec2 content = get_minimum_size();

    // The offset the alignment names, per axis. Center splits the leftover with
    // integer division, biasing an odd leftover toward the start edge.
    auto offset = [this](int extent, int block) {
        switch (content_alignment) {
            case ContentAlignment::Begin: return 0;
            case ContentAlignment::End:   return extent - block;
            case ContentAlignment::Center: break;
        }
        return (extent - block) / 2;
    };

    return Rect{
        Vec2{box.pos.x + offset(box.size.x, content.x),
             box.pos.y + offset(box.size.y, content.y)},
        content,
    };
}

} // namespace guayaba