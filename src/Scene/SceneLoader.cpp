#include <exception>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <guayaba/Core/NodeRef.hpp>
#include <guayaba/Nodes/UI/Control.hpp>
#include <guayaba/Scene/SceneLoader.hpp>
#include <guayaba/Scene/SceneValidation.hpp>
#include <guayaba/Scene/ThemeFormat.hpp>

#include "SceneInternal.hpp"

namespace guayaba {

namespace {

using detail::BuildContext;
using detail::ScenePropertyBinding;

std::string join_path(const std::string& parent, const std::string& child) {
    if (parent.empty()) return "/" + child;
    if (parent == "/") return "/" + child;
    return parent + "/" + child;
}

std::unique_ptr<Node> build_node(const SceneNodeDesc& description,
                                 const std::string& parent_path,
                                 BuildContext& context,
                                 bool collect_all_errors);

void apply_binding(const ScenePropertyBinding& binding,
                   Node& node,
                   const SceneValue& value,
                   BuildContext& context,
                   bool collect_all_errors,
                   bool& failed) {
    const std::size_t before =
        context.sink.diagnostics ? context.sink.diagnostics->size() : 0;
    try {
        if (!binding.setter(node, value, context)) {
            const std::size_t after =
                context.sink.diagnostics ? context.sink.diagnostics->size() : 0;
            if (after == before) {
                context.sink.error(SceneDiagnosticCode::PropertyRejected, value.span(),
                                   "property '" + binding.info.name + "' was rejected");
            }
            failed = true;
            if (!collect_all_errors) return;
        }
    } catch (const std::exception& error) {
        context.sink.error(SceneDiagnosticCode::PropertyRejected, value.span(),
                           "property '" + binding.info.name + "' was rejected: " +
                               error.what());
        failed = true;
    } catch (...) {
        context.sink.error(SceneDiagnosticCode::PropertyRejected, value.span(),
                           "property '" + binding.info.name + "' was rejected");
        failed = true;
    }
}

std::unique_ptr<Node> create_node(const SceneNodeDesc& description,
                                  const std::string& path,
                                  BuildContext& context,
                                  bool collect_all_errors,
                                  const detail::SceneTypeBinding& type,
                                  bool& failed) {
    std::unique_ptr<Node> node;
    try {
        node = type.factory(description.name);
    } catch (const std::exception& error) {
        context.sink.node_path = path;
        context.sink.error(SceneDiagnosticCode::PropertyRejected, description.span,
                           "cannot construct node: " + std::string(error.what()));
        failed = true;
        return nullptr;
    } catch (...) {
        context.sink.node_path = path;
        context.sink.error(SceneDiagnosticCode::PropertyRejected, description.span,
                           "cannot construct node");
        failed = true;
        return nullptr;
    }

    if (!node) {
        context.sink.node_path = path;
        context.sink.error(SceneDiagnosticCode::PropertyRejected, description.span,
                           "node factory returned null");
        failed = true;
        return nullptr;
    }

    if (context.apply_theme && context.theme != nullptr) {
        if (Control* control = dynamic_cast<Control*>(node.get())) {
            control->apply_theme(*context.theme);
        }
    }

    context.sink.node_path = path;
    for (const ScenePropertyInfo& property : type.info.properties) {
        const SceneProperty* source = nullptr;
        for (const SceneProperty& candidate : description.properties) {
            if (candidate.name == property.name) {
                source = &candidate;
                break;
            }
        }
        if (source == nullptr) continue;

        const auto binding = type.properties.find(property.name);
        if (binding == type.properties.end()) continue;

        apply_binding(binding->second, *node, source->value, context,
                      collect_all_errors, failed);
        if (failed && !collect_all_errors) return nullptr;
    }

    return node;
}

std::unique_ptr<Node> build_node(const SceneNodeDesc& description,
                                 const std::string& parent_path,
                                 BuildContext& context,
                                 bool collect_all_errors) {
    const std::string path = join_path(parent_path, description.name);
    context.sink.node_path = path;

    const detail::SceneSchemaRegistry& registry = detail::SceneSchemaRegistry::instance();
    const detail::SceneTypeBinding* type = registry.find(description.type);
    if (type == nullptr) {
        context.sink.error(SceneDiagnosticCode::UnknownType, description.span,
                           "unknown node type '" + description.type + "'");
        return nullptr;
    }

    bool failed = false;
    std::unique_ptr<Node> node = create_node(description, path, context,
                                             collect_all_errors, *type, failed);
    if (!node || failed) {
        if (!collect_all_errors) return nullptr;
    }
    if (!node) return nullptr;

    for (const std::unique_ptr<SceneNodeDesc>& child_description : description.children) {
        std::unique_ptr<Node> child = build_node(*child_description, path, context,
                                                 collect_all_errors);
        if (!child) {
            failed = true;
            if (!collect_all_errors) return nullptr;
            continue;
        }

        const std::string expected_name = child->get_name();
        node->add_child(std::move(child));
        Node* added = node->get_child<Node>(node->get_child_count() - 1).get();
        if (added != nullptr && added->get_name() != expected_name) {
            context.sink.node_path = path;
            context.sink.error(SceneDiagnosticCode::DuplicateSibling, child_description->span,
                               "node name '" + expected_name +
                                   "' was renamed to '" + added->get_name() +
                                   "' because a sibling already used it");
            failed = true;
            if (!collect_all_errors) return nullptr;
        }
    }

    context.sink.node_path = parent_path;
    return node;
}

} // namespace

SceneBuildResult build_scene(const SceneDocument& document,
                             const Theme* theme,
                             const SceneLoadOptions& options) {
    SceneBuildResult result;

    std::vector<SceneDiagnostic> validation =
        validate_scene(document, true, options.source_name);
    result.diagnostics.insert(result.diagnostics.end(),
                              std::make_move_iterator(validation.begin()),
                              std::make_move_iterator(validation.end()));

    if (has_error(result.diagnostics)) return result;

    BuildContext context;
    context.sink.source = options.source_name;
    context.sink.diagnostics = &result.diagnostics;
    context.theme = theme;
    context.apply_theme = options.apply_theme;

    std::unique_ptr<Node> root = build_node(*document.root, std::string(), context,
                                            options.collect_all_errors);
    if (root && !has_error(result.diagnostics)) {
        result.root = std::move(root);
    }
    return result;
}

SceneAsset SceneLoader::load_file(const std::filesystem::path& path,
                                  const SceneLoadOptions& options) {
    SceneParseOptions parse_options;
    parse_options.source_name = path.string();

    SceneParseResult parsed = parse_scene_file(path, parse_options);
    if (!parsed.ok()) throw SceneError(std::move(parsed.diagnostics));

    SceneLoadOptions load_options = options;
    if (load_options.source_name.empty() || load_options.source_name == "<memory>") {
        load_options.source_name = path.string();
    }
    if (load_options.base_dir.empty()) load_options.base_dir = path.parent_path();

    std::unique_ptr<Theme> owned_theme;
    const Theme* theme = load_options.fallback_theme;

    if (!parsed.document.theme.empty()) {
        std::filesystem::path theme_path = std::filesystem::u8path(parsed.document.theme);
        if (theme_path.is_relative()) theme_path = load_options.base_dir / theme_path;

        SceneParseOptions theme_parse_options;
        theme_parse_options.source_name = theme_path.string();
        ThemeParseResult parsed_theme = parse_theme_file(theme_path, theme_parse_options);
        if (!parsed_theme.ok()) throw SceneError(std::move(parsed_theme.diagnostics));

        ThemeBuildResult built_theme = build_theme(parsed_theme.document);
        if (!built_theme.ok()) throw SceneError(std::move(built_theme.diagnostics));

        owned_theme = std::move(built_theme.theme);
        theme = owned_theme.get();
    }

    SceneBuildResult built =
        build_scene(parsed.document, theme, load_options);
    if (!built.ok()) throw SceneError(std::move(built.diagnostics));

    SceneAsset asset;
    asset.root = std::move(built.root);
    asset.theme = std::move(owned_theme);
    return asset;
}

SceneAsset SceneLoader::load_string(std::string_view text,
                                    const SceneLoadOptions& options) {
    SceneParseOptions parse_options;
    parse_options.source_name = options.source_name.empty() ? "<memory>"
                                                             : options.source_name;

    SceneParseResult parsed = parse_scene(text, parse_options);
    if (!parsed.ok()) throw SceneError(std::move(parsed.diagnostics));

    std::unique_ptr<Theme> owned_theme;
    const Theme* theme = options.fallback_theme;

    if (!parsed.document.theme.empty()) {
        std::filesystem::path theme_path = std::filesystem::u8path(parsed.document.theme);
        if (theme_path.is_relative() && !options.base_dir.empty()) {
            theme_path = options.base_dir / theme_path;
        }

        SceneParseOptions theme_parse_options;
        theme_parse_options.source_name = theme_path.string();
        ThemeParseResult parsed_theme = parse_theme_file(theme_path, theme_parse_options);
        if (!parsed_theme.ok()) throw SceneError(std::move(parsed_theme.diagnostics));

        ThemeBuildResult built_theme = build_theme(parsed_theme.document);
        if (!built_theme.ok()) throw SceneError(std::move(built_theme.diagnostics));

        owned_theme = std::move(built_theme.theme);
        theme = owned_theme.get();
    }

    SceneBuildResult built = build_scene(parsed.document, theme, options);
    if (!built.ok()) throw SceneError(std::move(built.diagnostics));

    SceneAsset asset;
    asset.root = std::move(built.root);
    asset.theme = std::move(owned_theme);
    return asset;
}

} // namespace guayaba
