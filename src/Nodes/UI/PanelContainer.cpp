#include <algorithm>
#include <utility>

#include <guayaba/Nodes/UI/PanelContainer.hpp>

namespace guayaba {

PanelContainer::PanelContainer(const std::string& node_name, Vec2 node_position)
    : Container(node_name, node_position) {}

Rect PanelContainer::panel_content_rect() const {
    return stylebox.get_content_rect(get_rect());
}

Vec2 PanelContainer::get_minimum_size() const {
    // The border and margins are added on top of whatever the child needs, rather
    // than being treated as padding to be centred inside. They are the panel's
    // own frame, so a panel squeezed below them has to lose content rather than
    // lose its edges.
    //
    // Container::get_minimum_size() already collects the children's minimums, so
    // this only has to add the frame and re-apply the caller's own minimum on
    // top - the same floor, one level up.
    const Vec2 content = Container::get_minimum_size();
    const Vec2 frame = stylebox.get_frame_size();

    const Vec2 own = Control::get_minimum_size();
    return {std::max(own.x, content.x + frame.x), std::max(own.y, content.y + frame.y)};
}

void PanelContainer::update_layout(const Rect& parent_rect) {
    resolve_rect(parent_rect);

    // No content_rect() here: the child takes the whole area inside the frame
    // rather than a block centred inside it. get_rect() is already the panel's
    // own rect, so the offset places the child at the content rect's corner.
    const Rect content = panel_content_rect();
    for (size_t i = 0; i < children.size(); ++i) place_child(i, content);
}

void PanelContainer::draw(ConsoleRenderer& renderer, const Rect& clip) {
    // The panel itself is drawn exactly as large as it is and is clipped only by
    // whatever its ancestors allowed - clip_children governs what this node
    // contains, not whether the node itself is visible.
    stylebox.apply(renderer, get_rect(), clip);

    // Hand back to Control so the children are drawn with the clip it narrows
    // for them.
    Control::draw(renderer, clip);
}

} // namespace guayaba