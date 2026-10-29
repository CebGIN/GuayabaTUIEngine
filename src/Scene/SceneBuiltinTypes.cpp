#include <algorithm>
#include <climits>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <guayaba/Core/Input.hpp>
#include <guayaba/Nodes/Node.hpp>
#include <guayaba/Nodes/Node2D.hpp>
#include <guayaba/Nodes/NodeTextMatrix.hpp>
#include <guayaba/Nodes/Timer.hpp>
#include <guayaba/Nodes/UI/Button.hpp>
#include <guayaba/Nodes/UI/CheckBox.hpp>
#include <guayaba/Nodes/UI/Container.hpp>
#include <guayaba/Nodes/UI/Graph.hpp>
#include <guayaba/Nodes/UI/GridContainer.hpp>
#include <guayaba/Nodes/UI/HBox.hpp>
#include <guayaba/Nodes/UI/Label.hpp>
#include <guayaba/Nodes/UI/LineEdit.hpp>
#include <guayaba/Nodes/UI/ListView.hpp>
#include <guayaba/Nodes/UI/MarginContainer.hpp>
#include <guayaba/Nodes/UI/PanelContainer.hpp>
#include <guayaba/Nodes/UI/ProgressBar.hpp>
#include <guayaba/Nodes/UI/ScrollView.hpp>
#include <guayaba/Nodes/UI/SpinBox.hpp>
#include <guayaba/Nodes/UI/Spinner.hpp>
#include <guayaba/Nodes/UI/SplitContainer.hpp>
#include <guayaba/Nodes/UI/VBox.hpp>

#include "SceneInternal.hpp"
#include "SceneText.hpp"
#include "SceneValueUtils.hpp"

namespace guayaba::detail {

namespace {

using Bindings = std::vector<ScenePropertyBinding>;

const std::vector<std::string> ANCHOR_PRESETS{
    "top_left", "top", "top_right", "center", "left", "right",
    "bottom_left", "bottom", "bottom_right", "full_rect",
};

const std::vector<std::string> GROW_MODES{"none", "begin", "end", "both"};
const std::vector<std::string> FOCUS_MODES{"none", "click", "all"};
const std::vector<std::string> ALIGNMENTS{"begin", "center", "end"};
const std::vector<std::string> SPLIT_ORIENTATIONS{"vertical_divider", "horizontal_divider"};
const std::vector<std::string> TEXT_ALIGNMENTS{"left", "center", "right"};

SceneValue value_bool(bool value) { return SceneValue::boolean(value); }
SceneValue value_int(int value) { return SceneValue::integer(value); }
SceneValue value_float(double value) { return SceneValue::number(value); }
SceneValue value_string(const char* value) { return SceneValue::string(value); }
SceneValue value_color(const char* value) { return SceneValue::string(value); }
SceneValue value_null() { return SceneValue::null(); }
SceneValue value_array(std::vector<SceneValue> values) { return SceneValue::array(std::move(values)); }
SceneValue value_vec2(int x, int y) {
    return value_array({value_int(x), value_int(y)});
}
SceneValue value_range(double min, double max) {
    return value_array({value_float(min), value_float(max)});
}

ScenePropertyInfo property(std::string name,
                           ScenePropertyKind kind,
                           SceneValue default_value,
                           std::string category,
                           std::vector<std::string> enum_values = {},
                           bool nullable = false,
                           bool single_line = false) {
    ScenePropertyInfo info;
    info.name = std::move(name);
    info.kind = kind;
    info.default_value = std::move(default_value);
    info.enum_values = std::move(enum_values);
    info.nullable = nullable;
    info.single_line = single_line;
    info.category = std::move(category);
    return info;
}

ScenePropertyBinding binding(ScenePropertyInfo info, ScenePropertySetter setter) {
    return ScenePropertyBinding{std::move(info), std::move(setter)};
}

ScenePropertyInfo non_negative(ScenePropertyInfo info) {
    info.non_negative = true;
    return info;
}

template <typename T, typename Fn>
ScenePropertySetter bool_setter(const char* property_name, Fn fn) {
    return [property_name, fn](Node& node, const SceneValue& value, BuildContext& context) {
        const std::optional<bool> parsed = read_bool(value, context.sink, property_name);
        if (!parsed) return false;
        fn(static_cast<T&>(node), *parsed);
        return true;
    };
}

template <typename T, typename Fn>
ScenePropertySetter int_setter(const char* property_name, long long minimum,
                               long long maximum, Fn fn) {
    return [property_name, minimum, maximum, fn](Node& node, const SceneValue& value,
                                                 BuildContext& context) {
        const std::optional<long long> parsed =
            read_int(value, context.sink, property_name, minimum, maximum);
        if (!parsed) return false;
        fn(static_cast<T&>(node), *parsed);
        return true;
    };
}

template <typename T, typename Fn>
ScenePropertySetter float_setter(const char* property_name, double minimum,
                                 double maximum, Fn fn) {
    return [property_name, minimum, maximum, fn](Node& node, const SceneValue& value,
                                                 BuildContext& context) {
        const std::optional<double> parsed =
            read_float(value, context.sink, property_name, minimum, maximum);
        if (!parsed) return false;
        fn(static_cast<T&>(node), *parsed);
        return true;
    };
}

template <typename T, typename Fn>
ScenePropertySetter string_setter(const char* property_name, bool single_line, Fn fn) {
    return [property_name, single_line, fn](Node& node, const SceneValue& value,
                                            BuildContext& context) {
        const std::optional<std::string> parsed =
            read_string(value, context.sink, property_name, single_line);
        if (!parsed) return false;
        fn(static_cast<T&>(node), *parsed);
        return true;
    };
}

template <typename T, typename Fn>
ScenePropertySetter color_setter(const char* property_name, Fn fn) {
    return [property_name, fn](Node& node, const SceneValue& value, BuildContext& context) {
        const std::optional<std::uint16_t> parsed =
            read_color(value, context.sink, property_name);
        if (!parsed) return false;
        fn(static_cast<T&>(node), *parsed);
        return true;
    };
}

template <typename T, typename Fn>
ScenePropertySetter enum_setter(const char* property_name,
                                const std::vector<std::string>& choices,
                                Fn fn) {
    return [property_name, choices, fn](Node& node, const SceneValue& value,
                                        BuildContext& context) {
        const std::optional<std::string> parsed =
            read_enum(value, context.sink, property_name, choices);
        if (!parsed) return false;
        fn(static_cast<T&>(node), *parsed);
        return true;
    };
}

template <typename T, typename Fn>
ScenePropertySetter vec2_setter(const char* property_name, bool non_negative, Fn fn) {
    return [property_name, non_negative, fn](Node& node, const SceneValue& value,
                                             BuildContext& context) {
        const std::optional<Vec2> parsed =
            read_vec2(value, context.sink, property_name, non_negative);
        if (!parsed) return false;
        fn(static_cast<T&>(node), *parsed);
        return true;
    };
}

template <typename T, typename Fn>
ScenePropertySetter edge_setter(const char* property_name, Fn fn) {
    return [property_name, fn](Node& node, const SceneValue& value, BuildContext& context) {
        const std::optional<EdgeSize> parsed =
            read_edge_size(value, context.sink, property_name);
        if (!parsed) return false;
        fn(static_cast<T&>(node), *parsed);
        return true;
    };
}

template <typename T, typename Fn>
ScenePropertySetter style_setter(const char* property_name, Fn fn) {
    return [property_name, fn](Node& node, const SceneValue& value, BuildContext& context) {
        const std::optional<StyleBox> parsed =
            read_style(value, context.sink, context.theme, property_name);
        if (!parsed) return false;
        fn(static_cast<T&>(node), *parsed);
        return true;
    };
}

template <typename T>
std::unique_ptr<Node> make_node(const std::string& name) {
    return std::make_unique<T>(name);
}

AnchorPreset anchor_preset(const std::string& name) {
    if (name == "top") return AnchorPreset::Top;
    if (name == "top_right") return AnchorPreset::TopRight;
    if (name == "center") return AnchorPreset::Center;
    if (name == "left") return AnchorPreset::Left;
    if (name == "right") return AnchorPreset::Right;
    if (name == "bottom_left") return AnchorPreset::BottomLeft;
    if (name == "bottom") return AnchorPreset::Bottom;
    if (name == "bottom_right") return AnchorPreset::BottomRight;
    if (name == "full_rect") return AnchorPreset::FullRect;
    return AnchorPreset::TopLeft;
}

Grow grow_mode(const std::string& name) {
    if (name == "begin") return Grow::Begin;
    if (name == "end") return Grow::End;
    if (name == "both") return Grow::Both;
    return Grow::None;
}

Control::FocusMode focus_mode(const std::string& name) {
    if (name == "click") return Control::FocusMode::CLICK;
    if (name == "all") return Control::FocusMode::ALL;
    return Control::FocusMode::NONE;
}

ContentAlignment content_alignment(const std::string& name) {
    if (name == "begin") return ContentAlignment::Begin;
    if (name == "end") return ContentAlignment::End;
    return ContentAlignment::Center;
}

SplitOrientation split_orientation(const std::string& name) {
    return name == "horizontal_divider" ? SplitOrientation::HorizontalDivider
                                        : SplitOrientation::VerticalDivider;
}

TextAlign text_align(const std::string& name) {
    if (name == "center") return TextAlign::Center;
    if (name == "right") return TextAlign::Right;
    return TextAlign::Left;
}


std::optional<double> read_positive_float(const SceneValue& value,
                                          SceneDiagnosticSink& sink,
                                          const std::string& property) {
    const std::optional<double> parsed = read_float(value, sink, property, 0.0);
    if (!parsed) return std::nullopt;
    if (*parsed <= 0.0) {
        sink.error(SceneDiagnosticCode::ValueOutOfRange, value.span(),
                   "property '" + property + "' must be greater than zero");
        return std::nullopt;
    }
    return parsed;
}

ScenePropertyBinding focus_mode_binding(const char* default_mode) {
    return binding(
        property("focus_mode", ScenePropertyKind::Enum, value_string(default_mode),
                 "Layout", FOCUS_MODES),
        enum_setter<Control>("focus_mode", FOCUS_MODES,
                             [](Control& control, const std::string& name) {
                                 control.set_focus_mode(focus_mode(name));
                             }));
}

Bindings node_properties() {
    return {
        binding(property("visible", ScenePropertyKind::Bool, value_bool(true), "Node"),
                bool_setter<Node>("visible", [](Node& node, bool visible) {
                    node.set_visible(visible);
                })),
    };
}

Bindings node2d_properties() {
    return {
        binding(property("position", ScenePropertyKind::Vec2, value_vec2(0, 0), "Layout"),
                vec2_setter<Node2D>("position", false, [](Node2D& node, Vec2 position) {
                    node.set_local_position(position);
                })),
    };
}

Bindings control_properties() {
    return {
        binding(property("anchors", ScenePropertyKind::Enum, value_string("top_left"),
                         "Layout", ANCHOR_PRESETS),
                enum_setter<Control>("anchors", ANCHOR_PRESETS,
                                     [](Control& control, const std::string& name) {
                                         control.set_anchor_preset(anchor_preset(name));
                                     })),
        binding(property("anchor_left", ScenePropertyKind::Float, value_float(0.0), "Layout"),
                float_setter<Control>("anchor_left", -1.0e6, 1.0e6,
                                      [](Control& control, double ratio) {
                                          control.set_anchor_left(static_cast<float>(ratio));
                                      })),
        binding(property("anchor_right", ScenePropertyKind::Float, value_float(0.0), "Layout"),
                float_setter<Control>("anchor_right", -1.0e6, 1.0e6,
                                      [](Control& control, double ratio) {
                                          control.set_anchor_right(static_cast<float>(ratio));
                                      })),
        binding(property("anchor_top", ScenePropertyKind::Float, value_float(0.0), "Layout"),
                float_setter<Control>("anchor_top", -1.0e6, 1.0e6,
                                      [](Control& control, double ratio) {
                                          control.set_anchor_top(static_cast<float>(ratio));
                                      })),
        binding(property("anchor_bottom", ScenePropertyKind::Float, value_float(0.0), "Layout"),
                float_setter<Control>("anchor_bottom", -1.0e6, 1.0e6,
                                      [](Control& control, double ratio) {
                                          control.set_anchor_bottom(static_cast<float>(ratio));
                                      })),
        binding(non_negative(property("size", ScenePropertyKind::Vec2, value_vec2(0, 0),
                                      "Layout")),
                vec2_setter<Control>("size", true, [](Control& control, Vec2 size) {
                    control.set_size(size);
                })),
        binding(non_negative(property("custom_minimum_size", ScenePropertyKind::Vec2,
                                      value_vec2(0, 0), "Layout")),
                vec2_setter<Control>("custom_minimum_size", true,
                                     [](Control& control, Vec2 size) {
                                         control.set_custom_minimum_size(size);
                                     })),
        binding(property("grow_x", ScenePropertyKind::Enum, value_string("none"), "Layout",
                         GROW_MODES),
                enum_setter<Control>("grow_x", GROW_MODES,
                                     [](Control& control, const std::string& name) {
                                         control.set_grow_x(grow_mode(name));
                                     })),
        binding(property("grow_y", ScenePropertyKind::Enum, value_string("none"), "Layout",
                         GROW_MODES),
                enum_setter<Control>("grow_y", GROW_MODES,
                                     [](Control& control, const std::string& name) {
                                         control.set_grow_y(grow_mode(name));
                                     })),
        binding(property("clip_children", ScenePropertyKind::Bool, value_bool(false), "Layout"),
                bool_setter<Control>("clip_children", [](Control& control, bool enabled) {
                    control.set_clip_children(enabled);
                })),
        focus_mode_binding("none"),
    };
}

Bindings container_properties() {
    return {
        binding(property("content_alignment", ScenePropertyKind::Enum,
                         value_string("center"), "Layout", ALIGNMENTS),
                enum_setter<Container>("content_alignment", ALIGNMENTS,
                                       [](Container& container, const std::string& name) {
                                           container.set_content_alignment(content_alignment(name));
                                       })),
    };
}

Bindings hbox_properties() {
    return {
        binding(property("separation", ScenePropertyKind::Int, value_int(0), "Layout"),
                int_setter<HBox>("separation", 0, INT_MAX, [](HBox& box, long long value) {
                    box.set_separation(static_cast<int>(value));
                })),
    };
}

Bindings vbox_properties() {
    return {
        binding(property("separation", ScenePropertyKind::Int, value_int(0), "Layout"),
                int_setter<VBox>("separation", 0, INT_MAX, [](VBox& box, long long value) {
                    box.set_separation(static_cast<int>(value));
                })),
    };
}

Bindings grid_properties() {
    return {
        binding(property("columns", ScenePropertyKind::Int, value_int(1), "Layout"),
                int_setter<GridContainer>("columns", 1, INT_MAX,
                                          [](GridContainer& grid, long long value) {
                                              grid.set_columns(static_cast<int>(value));
                                          })),
        binding(property("h_separation", ScenePropertyKind::Int, value_int(0), "Layout"),
                int_setter<GridContainer>("h_separation", 0, INT_MAX,
                                          [](GridContainer& grid, long long value) {
                                              grid.set_h_separation(static_cast<int>(value));
                                          })),
        binding(property("v_separation", ScenePropertyKind::Int, value_int(0), "Layout"),
                int_setter<GridContainer>("v_separation", 0, INT_MAX,
                                          [](GridContainer& grid, long long value) {
                                              grid.set_v_separation(static_cast<int>(value));
                                          })),
    };
}

Bindings margin_properties() {
    return {
        binding(property("margins", ScenePropertyKind::EdgeSize,
                         value_array({value_int(0), value_int(0), value_int(0), value_int(0)}),
                         "Layout"),
                edge_setter<MarginContainer>("margins", [](MarginContainer& container,
                                                          EdgeSize margins) {
                    container.set_margins(margins);
                })),
    };
}

Bindings panel_properties() {
    return {
        binding(property("style", ScenePropertyKind::Style, value_null(), "Appearance",
                         {}, true),
                style_setter<PanelContainer>("style", [](PanelContainer& panel, StyleBox style) {
                    panel.set_stylebox(std::move(style));
                })),
    };
}

Bindings scroll_properties() {
    return {
        binding(property("track_style", ScenePropertyKind::Style, value_null(), "Appearance",
                         {}, true),
                style_setter<ScrollView>("track_style", [](ScrollView& view, StyleBox style) {
                    view.set_track_stylebox(std::move(style));
                })),
        binding(property("handle_style", ScenePropertyKind::Style, value_null(), "Appearance",
                         {}, true),
                style_setter<ScrollView>("handle_style", [](ScrollView& view, StyleBox style) {
                    view.set_handle_stylebox(std::move(style));
                })),
    };
}

Bindings split_properties() {
    return {
        binding(property("orientation", ScenePropertyKind::Enum,
                         value_string("vertical_divider"), "Layout", SPLIT_ORIENTATIONS),
                enum_setter<SplitContainer>("orientation", SPLIT_ORIENTATIONS,
                                            [](SplitContainer& split, const std::string& name) {
                                                split.set_orientation(split_orientation(name));
                                            })),
        binding(property("split_ratio", ScenePropertyKind::Float, value_float(0.5), "Layout"),
                float_setter<SplitContainer>("split_ratio", 0.0, 1.0,
                                             [](SplitContainer& split, double value) {
                                                 split.set_split_ratio(static_cast<float>(value));
                                             })),
        binding(property("min_first", ScenePropertyKind::Int, value_int(0), "Layout"),
                int_setter<SplitContainer>("min_first", 0, INT_MAX,
                                           [](SplitContainer& split, long long value) {
                                               split.set_min_first(static_cast<int>(value));
                                           })),
        binding(property("min_second", ScenePropertyKind::Int, value_int(0), "Layout"),
                int_setter<SplitContainer>("min_second", 0, INT_MAX,
                                           [](SplitContainer& split, long long value) {
                                               split.set_min_second(static_cast<int>(value));
                                           })),
        binding(property("divider_width", ScenePropertyKind::Int, value_int(1), "Layout"),
                int_setter<SplitContainer>("divider_width", 0, INT_MAX,
                                           [](SplitContainer& split, long long value) {
                                               split.set_divider_width(static_cast<int>(value));
                                           })),
        binding(property("divider_style", ScenePropertyKind::Style, value_null(),
                         "Appearance", {}, true),
                style_setter<SplitContainer>("divider_style",
                                             [](SplitContainer& split, StyleBox style) {
                                                 split.get_divider_stylebox() = std::move(style);
                                             })),
    };
}

Bindings label_properties() {
    return {
        binding(property("text", ScenePropertyKind::String, value_string(""), "Text"),
                string_setter<Label>("text", false, [](Label& label, const std::string& text) {
                    label.set_text(text);
                })),
        binding(property("fit_content", ScenePropertyKind::Bool, value_bool(false), "Layout"),
                bool_setter<Label>("fit_content", [](Label& label, bool fit) {
                    label.set_fit_content(fit);
                })),
        binding(property("text_color", ScenePropertyKind::Color, value_color("WHITE"),
                         "Appearance"),
                color_setter<Label>("text_color", [](Label& label, std::uint16_t color) {
                    label.set_text_color(color);
                })),
        binding(property("background_color", ScenePropertyKind::Color, value_color("BLACK"),
                         "Appearance"),
                color_setter<Label>("background_color", [](Label& label, std::uint16_t color) {
                    label.set_background_color(color);
                })),
        binding(property("style", ScenePropertyKind::Style, value_null(), "Appearance",
                         {}, true),
                style_setter<Label>("style", [](Label& label, StyleBox style) {
                    label.set_stylebox(std::move(style));
                })),
    };
}

Bindings button_properties() {
    return {
        binding(property("text", ScenePropertyKind::String, value_string(""), "Text"),
                string_setter<Button>("text", false, [](Button& button, const std::string& text) {
                    button.set_text(text);
                })),
        binding(property("text_align", ScenePropertyKind::Enum, value_string("left"), "Text",
                         TEXT_ALIGNMENTS),
                enum_setter<Button>("text_align", TEXT_ALIGNMENTS,
                                    [](Button& button, const std::string& name) {
                                        button.set_text_align(text_align(name));
                                    })),
        binding(property("disabled", ScenePropertyKind::Bool, value_bool(false), "State"),
                bool_setter<Button>("disabled", [](Button& button, bool disabled) {
                    button.set_disabled(disabled);
                })),
        binding(property("text_color.normal", ScenePropertyKind::Color, value_color("WHITE"),
                         "Appearance"),
                color_setter<Button>("text_color.normal",
                                     [](Button& button, std::uint16_t color) {
                                         button.set_text_color(ButtonState::Normal, color);
                                     })),
        binding(property("text_color.hover", ScenePropertyKind::Color, value_color("WHITE"),
                         "Appearance"),
                color_setter<Button>("text_color.hover",
                                     [](Button& button, std::uint16_t color) {
                                         button.set_text_color(ButtonState::Hover, color);
                                     })),
        binding(property("text_color.pressed", ScenePropertyKind::Color, value_color("WHITE"),
                         "Appearance"),
                color_setter<Button>("text_color.pressed",
                                     [](Button& button, std::uint16_t color) {
                                         button.set_text_color(ButtonState::Pressed, color);
                                     })),
        binding(property("text_color.disabled", ScenePropertyKind::Color,
                         value_color("BRIGHT_BLACK"), "Appearance"),
                color_setter<Button>("text_color.disabled",
                                     [](Button& button, std::uint16_t color) {
                                         button.set_text_color(ButtonState::Disabled, color);
                                     })),
        binding(property("style.normal", ScenePropertyKind::Style, value_null(),
                         "Appearance", {}, true),
                style_setter<Button>("style.normal", [](Button& button, StyleBox style) {
                    button.set_stylebox(ButtonState::Normal, std::move(style));
                })),
        binding(property("style.hover", ScenePropertyKind::Style, value_null(), "Appearance",
                         {}, true),
                style_setter<Button>("style.hover", [](Button& button, StyleBox style) {
                    button.set_stylebox(ButtonState::Hover, std::move(style));
                })),
        binding(property("style.pressed", ScenePropertyKind::Style, value_null(),
                         "Appearance", {}, true),
                style_setter<Button>("style.pressed", [](Button& button, StyleBox style) {
                    button.set_stylebox(ButtonState::Pressed, std::move(style));
                })),
        binding(property("style.disabled", ScenePropertyKind::Style, value_null(),
                         "Appearance", {}, true),
                style_setter<Button>("style.disabled", [](Button& button, StyleBox style) {
                    button.set_stylebox(ButtonState::Disabled, std::move(style));
                })),
        focus_mode_binding("click"),
    };
}

Bindings checkbox_properties() {
    return {
        binding(property("text", ScenePropertyKind::String, value_string(""), "Text"),
                string_setter<CheckBox>("text", true, [](CheckBox& box,
                                                         const std::string& text) {
                    box.set_text(text);
                })),
        binding(property("checked", ScenePropertyKind::Bool, value_bool(false), "State"),
                bool_setter<CheckBox>("checked", [](CheckBox& box, bool checked) {
                    box.set_checked(checked);
                })),
        binding(property("glyphs", ScenePropertyKind::GlyphPair,
                         value_array({value_string("x"), value_string(" ")}), "Appearance"),
                [](Node& node, const SceneValue& value, BuildContext& context) {
                    const std::optional<std::pair<char32_t, char32_t>> glyphs =
                        read_glyph_pair(value, context.sink, "glyphs");
                    if (!glyphs) return false;
                    static_cast<CheckBox&>(node).set_glyphs(glyphs->first, glyphs->second);
                    return true;
                }),
        binding(property("text_color", ScenePropertyKind::Color, value_color("WHITE"),
                         "Appearance"),
                color_setter<CheckBox>("text_color", [](CheckBox& box, std::uint16_t color) {
                    box.set_text_color(color);
                })),
        binding(property("background_color", ScenePropertyKind::Color, value_color("BLACK"),
                         "Appearance"),
                color_setter<CheckBox>("background_color", [](CheckBox& box,
                                                              std::uint16_t color) {
                    box.set_background_color(color);
                })),
        binding(property("style", ScenePropertyKind::Style, value_null(), "Appearance",
                         {}, true),
                style_setter<CheckBox>("style", [](CheckBox& box, StyleBox style) {
                    box.get_stylebox() = std::move(style);
                })),
        focus_mode_binding("click"),
    };
}

Bindings line_edit_properties() {
    return {
        binding(property("text", ScenePropertyKind::String, value_string(""), "Text"),
                string_setter<LineEdit>("text", true, [](LineEdit& edit,
                                                         const std::string& text) {
                    edit.set_text(text);
                })),
        binding(property("placeholder", ScenePropertyKind::String, value_string(""), "Text",
                         {}, false, true),
                string_setter<LineEdit>("placeholder", true, [](LineEdit& edit,
                                                                const std::string& text) {
                    edit.set_placeholder(text);
                })),
        binding(property("max_length", ScenePropertyKind::Int, value_int(0), "State"),
                int_setter<LineEdit>("max_length", 0, INT_MAX, [](LineEdit& edit,
                                                                 long long value) {
                    edit.set_max_length(static_cast<int>(value));
                })),
        binding(property("caret", ScenePropertyKind::Int, value_int(0), "State"),
                [](Node& node, const SceneValue& value, BuildContext& context) {
                    const std::optional<long long> caret =
                        read_int(value, context.sink, "caret", 0);
                    if (!caret) return false;
                    LineEdit& edit = static_cast<LineEdit&>(node);
                    const std::size_t codepoints = utf8_codepoint_count(edit.get_text());
                    if (static_cast<unsigned long long>(*caret) > codepoints) {
                        context.sink.error(SceneDiagnosticCode::ValueOutOfRange, value.span(),
                                           "property 'caret' is past the end of the text");
                        return false;
                    }
                    edit.set_caret(byte_offset_for_codepoint(edit.get_text(),
                                                             static_cast<std::size_t>(*caret)));
                    return true;
                }),
        binding(property("secret", ScenePropertyKind::Bool, value_bool(false), "State"),
                bool_setter<LineEdit>("secret", [](LineEdit& edit, bool secret) {
                    edit.set_secret(secret);
                })),
        binding(property("secret_character", ScenePropertyKind::Glyph, value_string("*"),
                         "Appearance"),
                [](Node& node, const SceneValue& value, BuildContext& context) {
                    const std::optional<char32_t> glyph =
                        read_glyph(value, context.sink, "secret_character");
                    if (!glyph) return false;
                    static_cast<LineEdit&>(node).set_secret_character(*glyph);
                    return true;
                }),
        binding(property("text_color", ScenePropertyKind::Color, value_color("WHITE"),
                         "Appearance"),
                color_setter<LineEdit>("text_color", [](LineEdit& edit, std::uint16_t color) {
                    edit.set_text_color(color);
                })),
        binding(property("placeholder_color", ScenePropertyKind::Color,
                         value_color("BRIGHT_BLACK"), "Appearance"),
                color_setter<LineEdit>("placeholder_color",
                                       [](LineEdit& edit, std::uint16_t color) {
                                           edit.set_placeholder_color(color);
                                       })),
        binding(property("background_color", ScenePropertyKind::Color, value_color("BLACK"),
                         "Appearance"),
                color_setter<LineEdit>("background_color",
                                       [](LineEdit& edit, std::uint16_t color) {
                                           edit.set_background_color(color);
                                       })),
        binding(property("style", ScenePropertyKind::Style, value_null(), "Appearance",
                         {}, true),
                style_setter<LineEdit>("style", [](LineEdit& edit, StyleBox style) {
                    edit.set_stylebox(std::move(style));
                })),
        focus_mode_binding("click"),
    };
}

Bindings spinbox_properties() {
    return {
        binding(property("range", ScenePropertyKind::Range, value_range(0.0, 100.0), "Data"),
                [](Node& node, const SceneValue& value, BuildContext& context) {
                    const std::optional<std::pair<double, double>> range =
                        read_range(value, context.sink, "range");
                    if (!range) return false;
                    static_cast<SpinBox&>(node).set_range(range->first, range->second);
                    return true;
                }),
        binding(property("step", ScenePropertyKind::Float, value_float(1.0), "Data"),
                [](Node& node, const SceneValue& value, BuildContext& context) {
                    const std::optional<double> step =
                        read_positive_float(value, context.sink, "step");
                    if (!step) return false;
                    static_cast<SpinBox&>(node).set_step(*step);
                    return true;
                }),
        binding(property("decimals", ScenePropertyKind::Int, value_int(0), "Data"),
                int_setter<SpinBox>("decimals", 0, 15, [](SpinBox& spin, long long value) {
                    spin.set_decimals(static_cast<int>(value));
                })),
        binding(property("value", ScenePropertyKind::Float, value_float(0.0), "Data"),
                float_setter<SpinBox>("value", -1.0e300, 1.0e300,
                                      [](SpinBox& spin, double value) {
                                          spin.set_value(value);
                                      })),
        binding(property("wrap", ScenePropertyKind::Bool, value_bool(false), "State"),
                bool_setter<SpinBox>("wrap", [](SpinBox& spin, bool wrap) {
                    spin.set_wrap(wrap);
                })),
        binding(property("style", ScenePropertyKind::Style, value_null(), "Appearance",
                         {}, true),
                style_setter<SpinBox>("style", [](SpinBox& spin, StyleBox style) {
                    const EdgeSize margin = style.get_content_margin();
                    style.set_content_margin(margin.left, margin.top, margin.right + 2,
                                             margin.bottom);
                    spin.set_stylebox(std::move(style));
                })),
    };
}

Bindings listview_properties() {
    return {
        binding(property("items", ScenePropertyKind::StringArray,
                         value_array({}), "Data", {}, false, true),
                [](Node& node, const SceneValue& value, BuildContext& context) {
                    const std::optional<std::vector<std::string>> items =
                        read_string_array(value, context.sink, "items", true);
                    if (!items) return false;
                    static_cast<ListView&>(node).set_items(*items);
                    return true;
                }),
        binding(property("selected", ScenePropertyKind::Int, value_int(-1), "State"),
                [](Node& node, const SceneValue& value, BuildContext& context) {
                    const std::optional<long long> selected =
                        read_int(value, context.sink, "selected", -1);
                    if (!selected) return false;
                    ListView& list = static_cast<ListView&>(node);
                    if (*selected >= 0 &&
                        static_cast<std::size_t>(*selected) >= list.get_items().size()) {
                        context.sink.error(SceneDiagnosticCode::ValueOutOfRange, value.span(),
                                           "property 'selected' is past the last item");
                        return false;
                    }
                    list.set_selected(static_cast<int>(*selected));
                    return true;
                }),
        binding(property("item_colors", ScenePropertyKind::IndexColorArray,
                         value_array({}), "Appearance"),
                [](Node& node, const SceneValue& value, BuildContext& context) {
                    const auto colors =
                        read_index_color_array(value, context.sink, "item_colors");
                    if (!colors) return false;
                    ListView& list = static_cast<ListView&>(node);
                    for (const auto& entry : *colors) {
                        if (static_cast<std::size_t>(entry.first) >= list.get_items().size()) {
                            context.sink.error(SceneDiagnosticCode::ValueOutOfRange,
                                               value.span(),
                                               "property 'item_colors' uses an index past the last item");
                            return false;
                        }
                    }
                    list.clear_item_colors();
                    for (const auto& entry : *colors) list.set_item_color(entry.first, entry.second);
                    return true;
                }),
        binding(property("text_color", ScenePropertyKind::Color, value_color("WHITE"),
                         "Appearance"),
                color_setter<ListView>("text_color", [](ListView& list, std::uint16_t color) {
                    list.set_text_color(color);
                })),
        binding(property("selection_background_color", ScenePropertyKind::Color,
                         value_color("BRIGHT_BLACK"), "Appearance"),
                color_setter<ListView>("selection_background_color",
                                       [](ListView& list, std::uint16_t color) {
                                           list.set_selection_background_color(color);
                                       })),
        binding(property("selection_text_color", ScenePropertyKind::Color,
                         value_color("BRIGHT_WHITE"), "Appearance"),
                color_setter<ListView>("selection_text_color",
                                       [](ListView& list, std::uint16_t color) {
                                           list.set_selection_text_color(color);
                                       })),
        binding(property("style", ScenePropertyKind::Style, value_null(), "Appearance",
                         {}, true),
                style_setter<ListView>("style", [](ListView& list, StyleBox style) {
                    list.set_stylebox(std::move(style));
                })),
        focus_mode_binding("click"),
    };
}

Bindings progress_properties() {
    return {
        binding(property("range", ScenePropertyKind::Range, value_range(0.0, 100.0), "Data"),
                [](Node& node, const SceneValue& value, BuildContext& context) {
                    const std::optional<std::pair<double, double>> range =
                        read_range(value, context.sink, "range");
                    if (!range) return false;
                    static_cast<ProgressBar&>(node).set_range(range->first, range->second);
                    return true;
                }),
        binding(property("value", ScenePropertyKind::Float, value_float(0.0), "Data"),
                float_setter<ProgressBar>("value", -1.0e300, 1.0e300,
                                          [](ProgressBar& bar, double value) {
                                              bar.set_value(value);
                                          })),
        binding(property("indeterminate", ScenePropertyKind::Bool, value_bool(false), "State"),
                bool_setter<ProgressBar>("indeterminate", [](ProgressBar& bar, bool enabled) {
                    bar.set_indeterminate(enabled);
                })),
        binding(property("text", ScenePropertyKind::String, value_string(""), "Text", {},
                         false, true),
                string_setter<ProgressBar>("text", true, [](ProgressBar& bar,
                                                            const std::string& text) {
                    bar.set_text(text);
                })),
        binding(property("show_percentage", ScenePropertyKind::Bool, value_bool(false),
                         "State"),
                bool_setter<ProgressBar>("show_percentage", [](ProgressBar& bar, bool show) {
                    bar.set_show_percentage(show);
                })),
        binding(property("fill_color", ScenePropertyKind::Color, value_color("GREEN"),
                         "Appearance"),
                color_setter<ProgressBar>("fill_color", [](ProgressBar& bar,
                                                            std::uint16_t color) {
                    bar.set_fill_color(color);
                })),
        binding(property("background_color", ScenePropertyKind::Color, value_color("BLACK"),
                         "Appearance"),
                color_setter<ProgressBar>("background_color",
                                          [](ProgressBar& bar, std::uint16_t color) {
                                              bar.set_background_color(color);
                                          })),
        binding(property("text_color", ScenePropertyKind::Color,
                         value_color("BRIGHT_WHITE"), "Appearance"),
                color_setter<ProgressBar>("text_color", [](ProgressBar& bar,
                                                           std::uint16_t color) {
                    bar.set_text_color(color);
                })),
        binding(property("style", ScenePropertyKind::Style, value_null(), "Appearance",
                         {}, true),
                style_setter<ProgressBar>("style", [](ProgressBar& bar, StyleBox style) {
                    bar.get_stylebox() = std::move(style);
                })),
    };
}

Bindings spinner_properties() {
    return {
        binding(property("frames", ScenePropertyKind::StringArray,
                         value_array({value_string("|"), value_string("/"),
                                      value_string("-"), value_string("\\")}),
                         "Data", {}, false, true),
                [](Node& node, const SceneValue& value, BuildContext& context) {
                    const std::optional<std::vector<std::string>> frames =
                        read_string_array(value, context.sink, "frames", true);
                    if (!frames) return false;
                    static_cast<Spinner&>(node).set_frames(*frames);
                    return true;
                }),
        binding(property("interval", ScenePropertyKind::Float, value_float(0.1), "Timing"),
                [](Node& node, const SceneValue& value, BuildContext& context) {
                    const std::optional<double> interval =
                        read_positive_float(value, context.sink, "interval");
                    if (!interval) return false;
                    static_cast<Spinner&>(node).set_interval(*interval);
                    return true;
                }),
        binding(property("running", ScenePropertyKind::Bool, value_bool(true), "State"),
                bool_setter<Spinner>("running", [](Spinner& spinner, bool running) {
                    spinner.set_running(running);
                })),
        binding(property("text_color", ScenePropertyKind::Color, value_color("WHITE"),
                         "Appearance"),
                color_setter<Spinner>("text_color", [](Spinner& spinner,
                                                       std::uint16_t color) {
                    spinner.set_text_color(color);
                })),
        binding(property("background_color", ScenePropertyKind::Color, value_color("BLACK"),
                         "Appearance"),
                color_setter<Spinner>("background_color",
                                      [](Spinner& spinner, std::uint16_t color) {
                                          spinner.set_background_color(color);
                                      })),
        binding(property("style", ScenePropertyKind::Style, value_null(), "Appearance",
                         {}, true),
                style_setter<Spinner>("style", [](Spinner& spinner, StyleBox style) {
                    spinner.get_stylebox() = std::move(style);
                })),
    };
}

Bindings graph_properties() {
    return {
        binding(property("values", ScenePropertyKind::FloatArray, value_array({}), "Data"),
                [](Node& node, const SceneValue& value, BuildContext& context) {
                    const std::optional<std::vector<double>> values =
                        read_float_array(value, context.sink, "values");
                    if (!values) return false;
                    static_cast<Graph&>(node).set_values(*values);
                    return true;
                }),
        binding(property("capacity", ScenePropertyKind::Int, value_int(0), "Data"),
                int_setter<Graph>("capacity", 0, INT_MAX, [](Graph& graph, long long value) {
                    graph.set_capacity(static_cast<int>(value));
                })),
        binding(property("range", ScenePropertyKind::Range, value_null(), "Data", {}, true),
                [](Node& node, const SceneValue& value, BuildContext& context) {
                    Graph& graph = static_cast<Graph&>(node);
                    if (value.kind() == SceneValueKind::Null) {
                        graph.clear_range();
                        return true;
                    }
                    const std::optional<std::pair<double, double>> range =
                        read_range(value, context.sink, "range");
                    if (!range) return false;
                    graph.set_range(range->first, range->second);
                    return true;
                }),
        binding(property("fill", ScenePropertyKind::Bool, value_bool(true), "Appearance"),
                bool_setter<Graph>("fill", [](Graph& graph, bool fill) {
                    graph.set_fill(fill);
                })),
        binding(property("line_color", ScenePropertyKind::Color, value_color("WHITE"),
                         "Appearance"),
                color_setter<Graph>("line_color", [](Graph& graph, std::uint16_t color) {
                    graph.set_line_color(color);
                })),
        binding(property("background_color", ScenePropertyKind::Color, value_color("BLACK"),
                         "Appearance"),
                color_setter<Graph>("background_color", [](Graph& graph, std::uint16_t color) {
                    graph.set_background_color(color);
                })),
    };
}

Bindings timer_properties() {
    return {
        binding(property("wait_time", ScenePropertyKind::Float, value_float(1.0), "Timing"),
                float_setter<Timer>("wait_time", 0.001, 1.0e300,
                                    [](Timer& timer, double seconds) {
                                        timer.set_wait_time(seconds);
                                    })),
        binding(property("one_shot", ScenePropertyKind::Bool, value_bool(false), "State"),
                bool_setter<Timer>("one_shot", [](Timer& timer, bool one_shot) {
                    timer.set_one_shot(one_shot);
                })),
        binding(property("autostart", ScenePropertyKind::Bool, value_bool(false), "State"),
                bool_setter<Timer>("autostart", [](Timer& timer, bool autostart) {
                    timer.set_autostart(autostart);
                })),
    };
}

Bindings text_matrix_properties() {
    return {
        binding(property("text", ScenePropertyKind::StringArray, value_array({}), "Text",
                         {}, false, true),
                [](Node& node, const SceneValue& value, BuildContext& context) {
                    const std::optional<std::vector<std::string>> lines =
                        read_string_array(value, context.sink, "text", true);
                    if (!lines) return false;
                    static_cast<NodeTextMatrix&>(node).set_text(*lines);
                    return true;
                }),
        binding(property("text_color", ScenePropertyKind::Color, value_color("WHITE"),
                         "Appearance"),
                color_setter<NodeTextMatrix>("text_color",
                                             [](NodeTextMatrix& matrix, std::uint16_t color) {
                                                 matrix.change_text_color(
                                                     canonical_color_name(color));
                                             })),
        binding(property("background_color", ScenePropertyKind::Color, value_color("BLACK"),
                         "Appearance"),
                color_setter<NodeTextMatrix>(
                    "background_color", [](NodeTextMatrix& matrix, std::uint16_t color) {
                        matrix.change_background_color(canonical_color_name(color));
                    })),
    };
}

} // namespace

void SceneSchemaRegistry::add_type(
    SceneTypeInfo info,
    std::function<std::unique_ptr<Node>(const std::string&)> factory,
    std::vector<ScenePropertyBinding> properties,
    const std::vector<std::string>& hidden) {
    SceneTypeBinding type;
    type.info.name = info.name;
    type.info.base = info.base;
    type.factory = std::move(factory);

    if (!info.base.empty()) {
        const SceneTypeBinding* base = find(info.base);
        if (base == nullptr) {
            throw std::logic_error("scene schema: base type '" + info.base +
                                   "' is not registered before '" + info.name + "'");
        }
        type.info.properties = base->info.properties;
        type.properties = base->properties;
    }

    for (const std::string& hidden_name : hidden) {
        type.properties.erase(hidden_name);
        type.info.properties.erase(
            std::remove_if(type.info.properties.begin(), type.info.properties.end(),
                           [&](const ScenePropertyInfo& property) {
                               return property.name == hidden_name;
                           }),
            type.info.properties.end());
    }

    for (ScenePropertyBinding& property : properties) {
        auto existing = type.properties.find(property.info.name);
        if (existing != type.properties.end()) {
            existing->second.info = property.info;
            existing->second.setter = std::move(property.setter);
            for (ScenePropertyInfo& candidate : type.info.properties) {
                if (candidate.name == property.info.name) candidate = property.info;
            }
            continue;
        }

        type.info.properties.push_back(property.info);
        type.properties.emplace(property.info.name, std::move(property));
    }

    types_.push_back(std::move(type));
}

const SceneSchemaRegistry& SceneSchemaRegistry::instance() {
    static const SceneSchemaRegistry registry;
    return registry;
}

const SceneTypeBinding* SceneSchemaRegistry::find(const std::string& name) const {
    for (const SceneTypeBinding& type : types_) {
        if (type.info.name == name) return &type;
    }
    return nullptr;
}

SceneSchemaRegistry::SceneSchemaRegistry() {
    add_type({"Node", "", {}}, make_node<Node>, node_properties());
    add_type({"Node2D", "Node", {}}, make_node<Node2D>, node2d_properties());
    add_type({"Control", "Node2D", {}}, make_node<Control>, control_properties());
    add_type({"Container", "Control", {}}, make_node<Container>, container_properties());

    add_type({"HBox", "Container", {}}, make_node<HBox>, hbox_properties());
    add_type({"VBox", "Container", {}}, make_node<VBox>, vbox_properties());
    add_type({"GridContainer", "Container", {}}, make_node<GridContainer>,
             grid_properties());
    add_type({"MarginContainer", "Container", {}}, make_node<MarginContainer>,
             margin_properties(), {"content_alignment"});
    add_type({"PanelContainer", "Container", {}}, make_node<PanelContainer>,
             panel_properties(), {"content_alignment"});
    add_type({"ScrollView", "PanelContainer", {}}, make_node<ScrollView>,
             scroll_properties(), {"content_alignment"});
    add_type({"SplitContainer", "Container", {}}, make_node<SplitContainer>,
             split_properties(), {"content_alignment"});

    add_type({"Label", "Control", {}}, make_node<Label>, label_properties());
    add_type({"Button", "Control", {}}, make_node<Button>, button_properties());
    add_type({"CheckBox", "Control", {}}, make_node<CheckBox>, checkbox_properties());
    add_type({"LineEdit", "Control", {}}, make_node<LineEdit>, line_edit_properties());
    add_type({"SpinBox", "LineEdit", {}}, make_node<SpinBox>, spinbox_properties(),
             {"text", "caret"});
    add_type({"ListView", "Control", {}}, make_node<ListView>, listview_properties());
    add_type({"ProgressBar", "Control", {}}, make_node<ProgressBar>,
             progress_properties());
    add_type({"Spinner", "Control", {}}, make_node<Spinner>, spinner_properties());
    add_type({"Graph", "Control", {}}, make_node<Graph>, graph_properties());

    add_type({"Timer", "Node", {}}, make_node<Timer>, timer_properties());
    add_type({"NodeTextMatrix", "Node2D", {}}, make_node<NodeTextMatrix>,
             text_matrix_properties());
}

} // namespace guayaba::detail

namespace guayaba {

SceneSchema::SceneSchema() {
    const detail::SceneSchemaRegistry& registry = detail::SceneSchemaRegistry::instance();
    types_.reserve(registry.types().size());
    for (const detail::SceneTypeBinding& type : registry.types()) {
        types_.push_back(type.info);
    }
}

const SceneSchema& SceneSchema::builtins() {
    static const SceneSchema schema;
    return schema;
}

const SceneTypeInfo* SceneSchema::find_type(std::string_view name) const {
    for (const SceneTypeInfo& type : types_) {
        if (type.name == std::string(name)) return &type;
    }
    return nullptr;
}

const ScenePropertyInfo* SceneSchema::find_property(std::string_view type,
                                                    std::string_view property) const {
    const SceneTypeInfo* info = find_type(type);
    if (info == nullptr) return nullptr;
    for (const ScenePropertyInfo& candidate : info->properties) {
        if (candidate.name == std::string(property)) return &candidate;
    }
    return nullptr;
}

} // namespace guayaba
