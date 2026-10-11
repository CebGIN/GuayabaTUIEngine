#include <algorithm>

#include <guayaba/Nodes/UI/Container.hpp>

namespace guayaba {

Container::Container(const std::string& node_name, Vec2 node_position)
    : Control(node_name, node_position) {}

void Container::update_layout(const Rect& parent_rect) {
    // resolve_rect rather than Control::update_layout: the base version would
    // hand our own rect down and then we would have to lay the children out a
    // second time against the centred one.
    resolve_rect(parent_rect);

    // Own rect first: the offset is measured against where we actually ended up,
    // and content_rect() reads get_rect().
    const Rect content = content_rect();
    for (size_t i = 0; i < children.size(); ++i) place_child(i, content);
}

void Container::place_child(size_t index, const Rect& rect) {
    Node* child = children[index].get();

    // Every positioned child is placed, not only the Controls: a NodeTextMatrix
    // is laid out against the same rect as anything else, so leaving it behind
    // would put it at our origin while its Controls sat in the middle. A plain
    // Node has no position at all and is skipped.
    if (Node2D* positioned = dynamic_cast<Node2D*>(child)) {
        positioned->set_local_position(rect.pos - get_global_position());
    }

    child->update_layout(rect);
}

Vec2 Container::get_minimum_size() const {
    const Vec2 own = Control::get_minimum_size();

    // The largest minimum size among the Control children. Reading their state
    // rather than their resolved rects is what makes this safe to call from
    // Control::resolve_rect, which calls it while computing our own size: the
    // walk bottoms out in the children's intrinsic state and never comes back
    // through our layout, so there is no cycle to bound.
    Vec2 content{0, 0};
    for (const std::unique_ptr<Node>& child : children) {
        const Control* as_control = dynamic_cast<const Control*>(child.get());
        if (as_control == nullptr) continue;

        const Vec2 child_minimum = as_control->get_minimum_size();
        content.x = std::max(content.x, child_minimum.x);
        content.y = std::max(content.y, child_minimum.y);
    }

    // The same floor Control applies between its own two minimums: neither can
    // shrink the other, so a Container forced bigger than its children - by
    // minimum_size or by its parent - pads the centred block instead of
    // collapsing to it.
    return {std::max(own.x, content.x), std::max(own.y, content.y)};
}

Rect Container::content_rect() const {
    const Rect box = get_rect();

    // Our own minimum size, which resolve_rect already floored the resolved size
    // at, so neither offset can be negative and the block cannot overflow us.
    const Vec2 content = get_minimum_size();

    return Rect{
        Vec2{box.pos.x + (box.size.x - content.x) / 2,
             box.pos.y + (box.size.y - content.y) / 2},
        content,
    };
}

} // namespace guayaba