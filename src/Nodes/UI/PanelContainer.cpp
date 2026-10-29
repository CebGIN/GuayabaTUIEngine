#include <algorithm>
#include <utility>

#include <guayaba/Nodes/UI/PanelContainer.hpp>
#include <guayaba/Resources/Theme.hpp>

namespace guayaba {

PanelContainer::PanelContainer(const std::string& node_name, Vec2 node_position)
    : Container(node_name, node_position) {}

Rect PanelContainer::panel_content_rect() const {
    return stylebox.get_content_rect(get_rect());
}

Vec2 PanelContainer::get_minimum_size() const {
    // The border and margins are added on top of the child's minimum, not centred
    // inside it: a panel squeezed below the frame loses content, not edges.
    // Control's own minimum floors it again.
    const Vec2 content = Container::get_minimum_size();
    const Vec2 frame = stylebox.get_frame_size();

    const Vec2 own = Control::get_minimum_size();
    return {std::max(own.x, content.x + frame.x), std::max(own.y, content.y + frame.y)};
}

void PanelContainer::on_update_layout(const Rect& parent_rect) {
    resolve_rect(parent_rect);

    // Not content_rect(): the child takes the whole area inside the frame, not a
    // centred block.
    const Rect content = panel_content_rect();
    for (size_t i = 0; i < children.size(); ++i) place_child(i, content);
}

void PanelContainer::on_draw(ConsoleRenderer& renderer, const Rect& clip) {
    // Drawn exactly as large as it is, clipped only by its ancestors.
    stylebox.apply(renderer, get_rect(), clip);

    // Hand back to Control so children use the clip it narrows for them.
    Control::on_draw(renderer, clip);
}

void PanelContainer::apply_theme(const Theme& theme) {
    stylebox = theme.get_style("panel", stylebox);
    invalidate_cache_recursively();
}

} // namespace guayaba