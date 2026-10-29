#include <algorithm>
#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <guayaba/Scene/SceneSchema.hpp>
#include <guayaba/Scene/SceneWriter.hpp>

#include "SceneFormatting.hpp"
#include "SceneIO.hpp"
#include "SceneText.hpp"

namespace guayaba {

namespace {

std::string indent_of(int width, int depth) {
    return std::string(static_cast<std::size_t>(width * depth), ' ');
}

std::string lower_ascii(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        out.push_back(c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c);
    }
    return out;
}

const SceneProperty* find_property(const SceneNodeDesc& node, std::string_view name) {
    for (const SceneProperty& property : node.properties) {
        if (property.name == name) return &property;
    }
    return nullptr;
}

bool has_individual_anchors(const SceneNodeDesc& node) {
    for (const SceneProperty& property : node.properties) {
        if (property.name.rfind("anchor_", 0) == 0) return true;
    }
    return false;
}

bool anchors_imply_both(const SceneNodeDesc& node) {
    const SceneProperty* anchors = find_property(node, "anchors");
    if (anchors == nullptr || anchors->value.kind() != SceneValueKind::String) return false;
    return lower_ascii(anchors->value.as_string()) == "full_rect";
}

SceneValue default_for_property(const ScenePropertyInfo& property,
                                const SceneNodeDesc& node) {
    if (property.name == "grow_x" || property.name == "grow_y") {
        if (find_property(node, "anchors") != nullptr) {
            return SceneValue::string(anchors_imply_both(node) ? "both" : "none");
        }
    }
    return property.default_value;
}

std::string render_color(const SceneValue& value) {
    if (value.kind() == SceneValueKind::String) {
        std::uint16_t color = 0;
        if (detail::parse_color_name(value.as_string(), color)) {
            return detail::color_to_source(color);
        }
    } else if (value.kind() == SceneValueKind::Int) {
        const long long color = value.as_int();
        if (color >= 0 && color <= 15) {
            return detail::color_to_source(static_cast<std::uint16_t>(color));
        }
    }
    return detail::value_to_source(value);
}

std::string index_color_array_to_source(const SceneValue& value) {
    if (value.kind() != SceneValueKind::Array) return detail::value_to_source(value);
    std::string out = "[";
    for (std::size_t i = 0; i < value.items().size(); ++i) {
        if (i != 0) out += ", ";
        const SceneValue& item = value.items()[i];
        if (item.kind() == SceneValueKind::Array && item.items().size() == 2) {
            out += '[';
            out += detail::value_to_source(item.items()[0]);
            out += ", ";
            out += render_color(item.items()[1]);
            out += ']';
        } else {
            out += detail::value_to_source(item);
        }
    }
    out += ']';
    return out;
}

std::string render_value(const ScenePropertyInfo& property, const SceneValue& value) {
    switch (property.kind) {
        case ScenePropertyKind::Enum:
            return detail::enum_to_source(value, property.enum_values);
        case ScenePropertyKind::Color:
            return render_color(value);
        case ScenePropertyKind::IndexColorArray:
            return index_color_array_to_source(value);
        case ScenePropertyKind::Style:
            return detail::style_to_source(value);
        case ScenePropertyKind::String:
        case ScenePropertyKind::Glyph:
            if (value.kind() == SceneValueKind::String) {
                return detail::string_to_source(value.as_string());
            }
            return detail::value_to_source(value);
        case ScenePropertyKind::Float:
            if (value.kind() == SceneValueKind::Int) {
                return detail::format_float(static_cast<double>(value.as_int()));
            }
            return detail::value_to_source(value);
        default:
            return detail::value_to_source(value);
    }
}

void structural_check(const SceneDocument& document, std::vector<SceneDiagnostic>& diagnostics) {
    auto error = [&](SceneDiagnosticCode code, std::string message) {
        diagnostics.push_back(make_diagnostic(code, "<memory>", {}, std::move(message)));
    };

    if (document.format_version != 1) {
        error(SceneDiagnosticCode::UnsupportedVersion,
              "cannot write scene format version " +
                  std::to_string(document.format_version) + "; this build writes 1");
    }
    if (!document.root) {
        error(SceneDiagnosticCode::MissingRoot, "cannot write a scene without a root node");
        return;
    }

    std::function<void(const SceneNodeDesc&)> visit = [&](const SceneNodeDesc& node) {
        std::string name_error;
        if (!detail::is_valid_node_name(node.name, &name_error)) {
            diagnostics.push_back(make_diagnostic(
                SceneDiagnosticCode::InvalidName, "<memory>", node.span,
                "cannot write invalid node name '" + node.name + "': " + name_error));
        }

        std::set<std::string> names;
        for (const std::unique_ptr<SceneNodeDesc>& child : node.children) {
            if (!names.insert(child->name).second) {
                diagnostics.push_back(make_diagnostic(
                    SceneDiagnosticCode::DuplicateSibling, "<memory>", child->span,
                    "cannot write duplicate sibling name '" + child->name + "'"));
            }
            visit(*child);
        }
    };
    visit(*document.root);
}

// Canonical .gscene writer: fixed header, schema property order, four-space
// indentation and one blank line between sibling blocks.
class Writer {
public:
    Writer(const SceneDocument& document, const SceneWriteOptions& options)
        : document_(document), options_(options) {}

    std::string run() {
        const int indent = options_.indent > 0 ? options_.indent : 4;
        std::string out;
        out += "format = 1\n";
        if (!document_.name.empty()) {
            out += "name = " + detail::string_to_source(document_.name) + "\n";
        }
        if (!document_.theme.empty()) {
            out += "theme = " + detail::string_to_source(document_.theme) + "\n";
        }
        out += '\n';

        write_node(*document_.root, 0, indent, out);

        if (!options_.trailing_newline && !out.empty() && out.back() == '\n') {
            out.pop_back();
        }
        return out;
    }

private:
    void write_node(const SceneNodeDesc& node, int depth, int indent_width,
                    std::string& out) {
        const std::string indent = indent_of(indent_width, depth);
        out += indent + node.type + " " + detail::string_to_source(node.name) + " {";

        std::vector<std::string> lines;
        const SceneTypeInfo* type = SceneSchema::builtins().find_type(node.type);

        if (type != nullptr) {
            const bool emit_anchors_default =
                options_.include_defaults && !has_individual_anchors(node);
            const bool skip_individual_anchor_defaults =
                options_.include_defaults && !has_individual_anchors(node);

            std::set<std::string> known;
            for (const ScenePropertyInfo& property : type->properties) {
                known.insert(property.name);

                const SceneProperty* value = find_property(node, property.name);
                if (value == nullptr) {
                    if (!options_.include_defaults) continue;
                    if (property.name == "anchors" && !emit_anchors_default) continue;
                    if (property.name.rfind("anchor_", 0) == 0 &&
                        skip_individual_anchor_defaults) {
                        continue;
                    }
                    const SceneValue fallback = default_for_property(property, node);
                    lines.push_back(property.name + " = " + render_value(property, fallback));
                    continue;
                }

                lines.push_back(property.name + " = " + render_value(property, value->value));
            }

            std::vector<const SceneProperty*> unknown;
            for (const SceneProperty& property : node.properties) {
                if (known.find(property.name) == known.end()) unknown.push_back(&property);
            }
            std::sort(unknown.begin(), unknown.end(),
                      [](const SceneProperty* lhs, const SceneProperty* rhs) {
                          return lhs->name < rhs->name;
                      });
            for (const SceneProperty* property : unknown) {
                lines.push_back(property->name + " = " +
                                detail::value_to_source(property->value));
            }
        } else {
            std::vector<const SceneProperty*> properties;
            properties.reserve(node.properties.size());
            for (const SceneProperty& property : node.properties) properties.push_back(&property);
            std::sort(properties.begin(), properties.end(),
                      [](const SceneProperty* lhs, const SceneProperty* rhs) {
                          return lhs->name < rhs->name;
                      });
            for (const SceneProperty* property : properties) {
                lines.push_back(property->name + " = " +
                                detail::value_to_source(property->value));
            }
        }

        if (lines.empty() && node.children.empty()) {
            out += "}\n";
            return;
        }

        out += '\n';
        const std::string inner = indent_of(indent_width, depth + 1);
        for (const std::string& line : lines) {
            out += inner + line + "\n";
        }

        for (std::size_t i = 0; i < node.children.size(); ++i) {
            if (!lines.empty() || i > 0) out += '\n';
            write_node(*node.children[i], depth + 1, indent_width, out);
        }

        out += indent + "}\n";
    }

    const SceneDocument& document_;
    SceneWriteOptions options_;
};

} // namespace

std::string write_scene(const SceneDocument& document, const SceneWriteOptions& options) {
    std::vector<SceneDiagnostic> diagnostics;
    structural_check(document, diagnostics);
    if (!diagnostics.empty()) throw SceneError(std::move(diagnostics));
    return Writer(document, options).run();
}

void write_scene_file(const SceneDocument& document,
                      const std::filesystem::path& path,
                      const SceneWriteOptions& options) {
    const std::string text = write_scene(document, options);
    std::string error;
    if (!detail::write_text_file(path, text, &error)) {
        throw SceneError({make_diagnostic(SceneDiagnosticCode::IoError, path.string(), {},
                                          std::move(error))});
    }
}

} // namespace guayaba
