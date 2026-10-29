#ifndef GUAYABA_SCENE_SCENEPARSER_HPP
#define GUAYABA_SCENE_SCENEPARSER_HPP

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <guayaba/Scene/SceneFormat.hpp>

namespace guayaba {

struct SceneParseOptions {
    std::string source_name = "<memory>";
    std::size_t max_bytes = 8 * 1024 * 1024;
    std::size_t max_nodes = 100000;
    std::size_t max_depth = 256;
};

struct SceneParseResult {
    SceneDocument document;
    std::vector<SceneDiagnostic> diagnostics;

    bool ok() const { return !has_error(diagnostics); }
};

SceneParseResult parse_scene(std::string_view text,
                             const SceneParseOptions& options = {});

SceneParseResult parse_scene_file(const std::filesystem::path& path,
                                  const SceneParseOptions& options = {});

} // namespace guayaba

#endif // GUAYABA_SCENE_SCENEPARSER_HPP
