#include <set>
#include <string>
#include <utility>
#include <vector>

#include <guayaba/Scene/SceneSchema.hpp>
#include <guayaba/Scene/SceneValidation.hpp>

#include "SceneInternal.hpp"
#include "SceneText.hpp"
#include "SceneValueUtils.hpp"

namespace guayaba {

namespace {

using detail::SceneDiagnosticSink;

bool validate_value(const ScenePropertyInfo& property,
                    const SceneValue& value,
                    SceneDiagnosticSink& sink) {
    switch (property.kind) {
        case ScenePropertyKind::Bool:
            return detail::read_bool(value, sink, property.name).has_value();
        case ScenePropertyKind::Int:
            return detail::read_int(value, sink, property.name).has_value();
        case ScenePropertyKind::Float:
            return detail::read_float(value, sink, property.name).has_value();
        case ScenePropertyKind::String:
            return detail::read_string(value, sink, property.name,
                                       property.single_line).has_value();
        case ScenePropertyKind::Color:
            return detail::read_color(value, sink, property.name).has_value();
        case ScenePropertyKind::Glyph:
            return detail::read_glyph(value, sink, property.name).has_value();
        case ScenePropertyKind::GlyphPair:
            return detail::read_glyph_pair(value, sink, property.name).has_value();
        case ScenePropertyKind::Vec2:
            return detail::read_vec2(value, sink, property.name, property.non_negative).has_value();
        case ScenePropertyKind::EdgeSize:
            return detail::read_edge_size(value, sink, property.name).has_value();
        case ScenePropertyKind::Range:
            if (value.kind() == SceneValueKind::Null) {
                if (property.nullable) return true;
                sink.error(SceneDiagnosticCode::TypeMismatch, value.span(),
                           "property '" + property.name + "' cannot be none");
                return false;
            }
            return detail::read_range(value, sink, property.name).has_value();
        case ScenePropertyKind::Enum:
            return detail::read_enum(value, sink, property.name,
                                     property.enum_values).has_value();
        case ScenePropertyKind::StringArray:
            return detail::read_string_array(value, sink, property.name,
                                             property.single_line).has_value();
        case ScenePropertyKind::FloatArray:
            return detail::read_float_array(value, sink, property.name).has_value();
        case ScenePropertyKind::IndexColorArray:
            return detail::read_index_color_array(value, sink, property.name).has_value();
        case ScenePropertyKind::Style:
            if (value.kind() == SceneValueKind::Null) {
                if (property.nullable) return true;
                sink.error(SceneDiagnosticCode::InvalidStyle, value.span(),
                           "property '" + property.name + "' cannot be none");
                return false;
            }
            if (value.kind() == SceneValueKind::String) {
                if (value.as_string().empty()) {
                    sink.error(SceneDiagnosticCode::InvalidStyle, value.span(),
                               "property '" + property.name +
                                   "' expects a non-empty style name");
                    return false;
                }
                return true;
            }
            return detail::read_style_object(value, sink, property.name).has_value();
    }
    return false;
}

void validate_node(const SceneNodeDesc& node,
                   const std::string& parent_path,
                   bool strict,
                   SceneDiagnosticSink& sink) {
    const std::string node_path =
        (parent_path == "/" ? "/" : parent_path + "/") + node.name;
    sink.node_path = node_path;

    std::string name_error;
    if (!detail::is_valid_node_name(node.name, &name_error)) {
        sink.error(SceneDiagnosticCode::InvalidName, node.span,
                   "invalid node name: " + name_error);
    }

    const SceneSchema& schema = SceneSchema::builtins();
    const SceneTypeInfo* type = schema.find_type(node.type);
    if (type == nullptr) {
        const SceneDiagnosticCode code = SceneDiagnosticCode::UnknownType;
        const std::string message = "unknown node type '" + node.type + "'";
        if (strict) sink.error(code, node.span, message);
        else sink.warning(code, node.span, message);
    }

    std::set<std::string> property_names;
    bool has_anchor_preset = false;
    bool has_individual_anchor = false;

    for (const SceneProperty& property : node.properties) {
        if (!property_names.insert(property.name).second) {
            sink.error(SceneDiagnosticCode::DuplicateProperty, property.span,
                       "duplicate property '" + property.name + "'");
        }

        if (type == nullptr) continue;

        const ScenePropertyInfo* info =
            schema.find_property(type->name, property.name);
        if (info == nullptr) {
            const SceneDiagnosticCode code = SceneDiagnosticCode::UnknownProperty;
            const std::string message =
                "unknown property '" + property.name + "' for " + type->name;
            if (strict) sink.error(code, property.span, message);
            else sink.warning(code, property.span, message);
            continue;
        }

        if (property.name == "anchors") {
            if (has_individual_anchor) {
                sink.error(SceneDiagnosticCode::InvalidDocument, property.span,
                           "property 'anchors' cannot be combined with 'anchor_*'");
            }
            has_anchor_preset = true;
        } else if (property.name.rfind("anchor_", 0) == 0) {
            if (has_anchor_preset) {
                sink.error(SceneDiagnosticCode::InvalidDocument, property.span,
                           "property 'anchors' cannot be combined with 'anchor_*'");
            }
            has_individual_anchor = true;
        }

        validate_value(*info, property.value, sink);
    }

    std::set<std::string> child_names;
    for (const std::unique_ptr<SceneNodeDesc>& child : node.children) {
        if (!child_names.insert(child->name).second) {
            sink.error(SceneDiagnosticCode::DuplicateSibling, child->span,
                       "duplicate sibling name '" + child->name + "'");
        }
        validate_node(*child, node_path, strict, sink);
    }

    sink.node_path = parent_path;
}

} // namespace

std::vector<SceneDiagnostic> validate_scene(const SceneDocument& document,
                                            bool strict,
                                            const std::string& source) {
    std::vector<SceneDiagnostic> diagnostics;
    SceneDiagnosticSink sink;
    sink.source = source;
    sink.diagnostics = &diagnostics;

    if (document.format_version != 1) {
        sink.error(SceneDiagnosticCode::UnsupportedVersion, {},
                   "unsupported scene format version " +
                       std::to_string(document.format_version) + "; this build supports 1");
    }

    if (!document.root) {
        sink.error(SceneDiagnosticCode::MissingRoot, {},
                   "the document has no root node");
        return diagnostics;
    }

    validate_node(*document.root, "/", strict, sink);
    return diagnostics;
}

} // namespace guayaba
