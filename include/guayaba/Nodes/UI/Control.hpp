#ifndef GUAYABA_NODES_UI_CONTROL_HPP
#define GUAYABA_NODES_UI_CONTROL_HPP

#include <string>

#include <guayaba/Core/Rect.hpp>
#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/Node.hpp>
#include <guayaba/Nodes/Node2D.hpp>
#include <guayaba/Renderer/ConsoleRenderer.hpp>

namespace guayaba {

/*
    Base class for nodes that lay themselves out.

    A Control is given a rectangle by its parent - the console buffer at the
    root, the enclosing Control's own rect below that - and turns its anchors
    into a position and a size inside it:

        anchor 0.0   = the left/top edge of the parent rect
        anchor 1.0   = the right/bottom edge of the parent rect

    The two horizontal anchors are independent of the two vertical ones, so
    anchor_left = 0 and anchor_right = 1 spans the full width. AnchorPreset
    just names the common combinations; the ratios are always the real state.

    Layout runs once per frame from SceneManager, before draw(). See
    Node::update_layout.
*/

// Named anchor combinations. set_anchor_preset() writes these into the four
// ratios, and get_anchor_preset() reads them back; anything the presets do not
// cover is still reachable through the individual setters.
enum class AnchorPreset {
    TopLeft,
    Top,
    TopRight,
    Center,
    Left,
    Right,
    BottomLeft,
    Bottom,
    BottomRight,
    FullRect,
};

// How a node is placed inside the rect its anchors name.
//
// Grow::None centers the node in the anchor rect. That is the useful default
// for the point presets: Top is anchor (0.5, 0), a degenerate rect at the middle
// of the top edge, and centering puts the node exactly there. Grow::None does
// not mean "collapse", and it never shrinks the node below minimum size.
enum class Grow {
    None,    // keep the size, centred in the anchor rect
    Begin,   // keep the size, pinned to the start edge (left/top)
    End,     // keep the size, pinned to the end edge (right/bottom)
    Both,    // stretch to fill the anchor rect
};

class Control : public Node2D {
protected:
    // Continuous anchors: 0.0 is the start edge of the parent rect, 1.0 the end.
    float anchor_left = 0.0f;
    float anchor_right = 0.0f;
    float anchor_top = 0.0f;
    float anchor_bottom = 0.0f;

    Vec2 minimum_size = {0, 0};

    Vec2 custom_minimum_size = {0, 0};

    Vec2 size = {0, 0};

    Grow grow_x = Grow::None;
    Grow grow_y = Grow::None;

    bool clip_children = false;

    Rect cached_rect;

    void set_minimum_size(Vec2 new_minimum_size);

    // Turns the anchors into cached_rect without touching the children.
    // update_layout() does that and then hands cached_rect down; a subclass that
    // needs its children to resolve against some other rect - see
    // Container::update_layout - calls this instead and propagates itself.
    void resolve_rect(const Rect& parent_rect);

public:
    Control(const std::string& node_name = "Unnamed Node", Vec2 node_position = {0, 0});

    void update_layout(const Rect& parent_rect) override;

    void draw(ConsoleRenderer& renderer, const Rect& clip) override;

    void invalidate_cache_recursively() override;

    // --- Anchors -------------------------------------------------------------

    AnchorPreset get_anchor_preset() const;

    void set_anchor_preset(AnchorPreset preset);

    float get_anchor_left() const { return anchor_left; }
    float get_anchor_right() const { return anchor_right; }
    float get_anchor_top() const { return anchor_top; }
    float get_anchor_bottom() const { return anchor_bottom; }

    void set_anchor_left(float ratio);
    void set_anchor_right(float ratio);
    void set_anchor_top(float ratio);
    void set_anchor_bottom(float ratio);

    // --- Size ----------------------------------------------------------------

    virtual Vec2 get_minimum_size() const;

    Vec2 get_custom_minimum_size() const { return custom_minimum_size; }

    void set_custom_minimum_size(Vec2 new_custom_minimum_size);

    Vec2 get_size() const { return cached_rect.size; }

    void set_size(Vec2 new_size);

    Grow get_grow_x() const { return grow_x; }
    Grow get_grow_y() const { return grow_y; }

    void set_grow_x(Grow grow);
    void set_grow_y(Grow grow);

    // --- Geometry ------------------------------------------------------------

    // Global-space rect from the last layout pass.
    Rect get_rect() const { return cached_rect; }

    bool is_inside(Vec2 point) const;

    // --- Clipping ------------------------------------------------------------

    bool get_clip_children() const { return clip_children; }

    void set_clip_children(bool enabled);

    Rect get_child_clip(const Rect& incoming_clip) const;
};

} // namespace guayaba

#endif // GUAYABA_NODES_UI_CONTROL_HPP