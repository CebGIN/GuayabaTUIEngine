#ifndef GUAYABA_SCENE_SCENELOADER_HPP
#define GUAYABA_SCENE_SCENELOADER_HPP

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <guayaba/Nodes/Node.hpp>
#include <guayaba/Resources/Theme.hpp>
#include <guayaba/Scene/SceneFormat.hpp>
#include <guayaba/Scene/SceneParser.hpp>

namespace guayaba {

struct SceneLoadOptions {
    // Source name used in validation/build diagnostics. load_file() fills it with
    // the scene path; load_string() leaves it at "<memory>" unless set.
    std::string source_name = "<memory>";

    // Directory used to resolve a relative "theme" path. load_file() defaults it
    // to the scene's parent directory; load_string() leaves it empty unless the
    // caller provides one.
    std::filesystem::path base_dir;

    // Applied when the scene does not declare its own theme. Not owned.
    const Theme* fallback_theme = nullptr;

    // Whether the resolved theme is applied to each control before its explicit
    // properties. The theme is still available for named style references when
    // this is false.
    bool apply_theme = true;

    // Stop after the first semantic error instead of collecting the whole tree.
    bool collect_all_errors = true;
};

struct SceneAsset {
    std::unique_ptr<Node> root;
    std::unique_ptr<Theme> theme; // Owned only when the scene declared one.
};

struct SceneBuildResult {
    std::unique_ptr<Node> root;
    std::vector<SceneDiagnostic> diagnostics;

    bool ok() const { return root != nullptr && !has_error(diagnostics); }
};

/*
    Builds a tree from a parsed document. The theme is not owned by the result;
    load_file() keeps it alive in SceneAsset::theme.
*/
SceneBuildResult build_scene(const SceneDocument& document,
                             const Theme* theme,
                             const SceneLoadOptions& options = {});

/*
    Parse + theme resolution + build. Throws SceneError when any stage fails.
    The returned root is detached; the caller decides when to change_scene().
*/
class SceneLoader {
public:
    static SceneAsset load_file(const std::filesystem::path& path,
                                const SceneLoadOptions& options = {});

    static SceneAsset load_string(std::string_view text,
                                  const SceneLoadOptions& options = {});
};

} // namespace guayaba

#endif // GUAYABA_SCENE_SCENELOADER_HPP
